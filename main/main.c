/******************************************************************************************
 *  Projeto final: Sistema embarcado de monitoramento e controle de estufa
 *  Autor: Luca Alves Cunha Schiavi
 *  GitHub: https://github.com/LucaSchiavi04
 *  
 * 
 *  Este código é um projeto final de um curso para sistemas embarcados utilizando ESP32
 *  (O maior foco desse projeto foi com ESP32-S3 N16R8, mas deve funcionar com outros 
 *  modelos, caso sejam feitas as devidas alterações).
 *  
 *  O objetivo do projeto é desenvolver um sistema de monitoramento e controle de uma 
 *  estufa, utilizando sensores para medir luminosidade, temperatura e umidade, além
 *  de permitir o controle de um ventilador com o objetivo de controlar a temperatura 
 *  interna da estufa.
 *  
 *  O dispositivo se interage com o usuário através de um display OLED (SSD1306) e o 
 *  operador pode interagir com o menu no display por meio dos botões conectados ao ESP32. 
 *  O operador pode monitorar informações em tempo real de temperatura, umidade, 
 *  luminosidade, estado da ventoinha e o setpoint de temperatura. Além disso, o operador 
 *  pode alterar o setpoint de temperatura, habilitar/desabilitar o controle de temperatura
 *  habilitar/desabilitar o registro de dados na memória.
 *
 *  Como acabou de ser dito acima, o sistema permite o registro de dados na memória, o qual
 *  permite que sejam salvos as últimas configurações realizadas do setpoint de temperatura, 
 *  estado do registro de dados e estado do controle de temperatura. Além das configurações,
 *  o sistema também registra os dados de monitoramento de temperatura, umidade e 
 *  luminosidade, dizendo em que momento determinado registro aconteceu desde a inicialização 
 *  do microcontrolador.
 *
 *****************************************************************************************/

/*
 *  Componentes próprias e externas.
 *  
 *  sensors.h:  Componente próprio para gerenciar e coletar dados dos sensores.
 *              Utiliza a biblioteca dht.h para o sensor DHT11 e a biblioteca 
 *              adc_oneshot.h para o sensor LDR.
 * 
 *  display.h:  Componente próprio para gerenciar o display OLED SSD1306.
 *              Utiliza a biblioteca ssd1306.h para o display.
 * 
 *  register.h: Componente próprio para gerenciar a memória flash do dispositivo.
 *              Utiliza a bliblioteca esp_littlefs.h.
 * 
 */
#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "sensors.h"
#include "display.h"
#include "register.h"


/*
 *  A definição dos diversos elementos a seguir deve ser feita a partir do Menuconfig.
 *  Lá, o usuário pode definir os pinos desejados para os botões, sensores, display e 
 *  atuadores. Além de definir configurações iniciais como setpoint de temperatura e 
 *  status inicial do registro de dados e controle de temperatura.
 */

// Definições de pinos de botões.
#define BTN6_PIN ((gpio_num_t) CONFIG_BUZZER_BUTTON_PIN)
#define BTN5_PIN ((gpio_num_t) CONFIG_SAVE_LOG_BUTTON_PIN)
#define BTN4_PIN ((gpio_num_t) CONFIG_DOWN_BUTTON_PIN)
#define BTN3_PIN ((gpio_num_t) CONFIG_RIGHT_BUTTON_PIN)
#define BTN2_PIN ((gpio_num_t) CONFIG_LEFT_BUTTON_PIN)
#define BTN1_PIN ((gpio_num_t) CONFIG_UP_BUTTON_PIN)

#define FAN_PIN ((gpio_num_t) CONFIG_FAN_PIN) // Pino do ventilador para refrigeração.

// Definição do pino do ADC.
#if CONFIG_SENSORS_LDR_ADC_UNIT_1
    #define LDR_UNIT ((adc_unit_t) ADC_UNIT_1)
#else
    #define LDR_UNIT ((adc_unit_t) ADC_UNIT_2)
#endif
#define LDR_CHANNEL ((adc_channel_t) CONFIG_SENSORS_LDR_ADC_CHANNEL)

#define DHT11_PIN ((gpio_num_t) CONFIG_DHT11_PIN) // Pino do sensor DHT11.

// Definições de pinos do display OLED SSD1306.
#define SDA_PIN ((int16_t) CONFIG_DISPLAY_SDA_GPIO)
#define SCL_PIN ((int16_t) CONFIG_DISPLAY_SCL_GPIO)
#define RESET_PIN ((int16_t) CONFIG_DISPLAY_RESET_GPIO)

// Definições dos estados da máquina de estados do display.
#define MENU_STATE ((char) '0')
#define MONITOR_STATE ((char) '1')
#define SETPOINT_STATE ((char) '2')
#define REGISTER_STATE ((char) '3')
#define CONTROLLER_STATE ((char) '4')
#define INSTRUCTIONS_STATE ((char) '5')
#define ABOUT_STATE ((char) '6')


/*
 *  Variáveis globais do sistema. Algumas variáveis podem ser alteradas pelo Menuconfig ou
 *  mediante o display OLED.
 */
const static char *TAG = "LOG";
volatile char machine_state = '0'; // Variável que diz o estado atual da máquina de estados do display.
volatile uint8_t menu_position = 0b00000001; // Variável que diz a posição atual do menu no display.
static int bright = 0; // Variável que armazena o valor bruto do sensor LDR.
static float humid = 0, temp = 0; // Variáveis que armazenam os valores de umidade e temperatura do sensor DHT11.
static float temp_setpoint; // Setpoint de temperatura inicial.
static float new_temp_setpoint = 0; // Variável temporária que armazena o novo setpoint antes de ser confirmado pelo operador.
static bool register_status; // Variável que armazena o status do registro de dados.
static bool control_status; // Variável que armazena o status do controle de temperatura.
static bool fan_status = 0; // Variável que armazena o status do ventilador.
static QueueHandle_t gpio_evt_queue = NULL; // "Manuseador" da fila de interrupções de botões.


// Declaração de funções relacionadas a interrupções de botões e manuseamento do display.
static void IRAM_ATTR gpio_isr_handler(void* arg);
void buttonTask(void *pvparameters); 


// Código principal do sistema embarcado.
void app_main(void)
{
    // Configuração do display OLED SSD1306 e mostra as instruções de uso.
    ESP_LOGI(TAG, "Inicializando sistema...");
    display_config_init(SDA_PIN, SCL_PIN, RESET_PIN);
    display_instructions();
    // Enquanto o operador visualiza as instruções, o sistema inicializa.

    gpio_config_t io_config = { // Struct de configuração dos pinos dos botões.
        .pin_bit_mask = (1ULL << BTN1_PIN)|(1ULL << BTN2_PIN)|(1ULL << BTN3_PIN)|
        (1ULL << BTN4_PIN)|(1ULL << BTN5_PIN)|(1ULL << BTN6_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };  
    ESP_ERROR_CHECK(gpio_config(&io_config)); // Configura e verifica se a configuração dos botões foi bem sucedida.

    // Configura as interrupções dos botões e verifica se a configuração foi bem sucedida.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN1_PIN, gpio_isr_handler, (void*)BTN1_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN2_PIN, gpio_isr_handler, (void*)BTN2_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN3_PIN, gpio_isr_handler, (void*)BTN3_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN4_PIN, gpio_isr_handler, (void*)BTN4_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN5_PIN, gpio_isr_handler, (void*)BTN5_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN6_PIN, gpio_isr_handler, (void*)BTN6_PIN));

    // Configura o pino da ventoinha.
    ESP_ERROR_CHECK(gpio_reset_pin(FAN_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(FAN_PIN, GPIO_MODE_OUTPUT));

    sensors_config_init(LDR_UNIT, LDR_CHANNEL); // Configura sensores.

    // Inicializa os sistemas de armazenamento e carrega as últimas configurações salvas na NVS (SP, REG_STATUS, CTRL_STATUS).
    register_init();
    nvs_load_last_config(&temp_setpoint, &register_status, &control_status);
    new_temp_setpoint = temp_setpoint; // Inicializa a variável temporária com o valor do setpoint inicial.

    vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos para que o operador visualize as instruções no display.

    gpio_evt_queue = xQueueCreate(1, sizeof(uint32_t)); // Cria a fila de interrupções de botões.
    xTaskCreate(buttonTask, "Button Task", 4096, NULL, 2, NULL); // Cria a tarefa que trata as interrupções de botões.

    ESP_LOGI(TAG, "Sistema inicializado.");

    // Inicializa o display com o menu principal.
    display_clear();
    display_menu(menu_position);

    
    // Loop principal.
    while(1) {
        sensors_read_value(&bright, LDR_CHANNEL, &humid, &temp, DHT11_PIN); // Leitura dos sensores.
        if(control_status) { // Se o controle de temperatura estiver habilitado, verifica se a ventoinha deve ligar.
            gpio_set_level(FAN_PIN, fan_status = (temp > temp_setpoint ? 1 : 0)); // Verifica se a ventoinha deve ligar.
        } else {
            gpio_set_level(FAN_PIN, fan_status = 0);
        }
        
        if(register_status) { // Se o registro de dados estiver habilitado, registra os dados na memória.
            logger_write(temp, humid, bright);
        }
        
        ESP_LOGI(TAG, "Temperatura: %.0f°C, Umidade: %.0f%%, Luminosidade: %d", temp, humid, bright); // Monitora os valores lidos.

        // Se o estado da máquina for MONITOR_STATE, atualiza o display com os valores lidos.
        if(machine_state == MONITOR_STATE) { 
            display_monitor(bright, humid, temp, temp_setpoint, fan_status);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay de atualização do display e leitura dos sensores.
    }
}

// Rotina de interrupção dos botões, envia o pino do botão pressionado para a fila.
static void IRAM_ATTR gpio_isr_handler(void* arg) { 
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Tarefa que trata as interrupções dos botões e atualiza a máquina de estado do display.
void buttonTask(void *pvparameters) {
    uint32_t gpio_num;
    TickType_t last_button_press = 0;

    while(1) {
        xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY);

        TickType_t current_time = xTaskGetTickCount();
        if(current_time - last_button_press >= pdMS_TO_TICKS(250)) {
            last_button_press = current_time; 

            if(gpio_num == BTN5_PIN) { // Se o botão de salvar log for pressionado, imprime o log no console serial.
                logger_read();
                continue; // Não altera o estado da máquina.
            }

            // Dependendo de onde o operador estiver no menu, o botão pressionado terá uma função diferente.
            switch (machine_state)
            {
            case MENU_STATE:
                if(gpio_num == BTN3_PIN) {
                    display_clear();
                    if(menu_position == 0b00000001) {
                        machine_state = MONITOR_STATE;
                        display_monitor(bright, humid, temp, temp_setpoint, fan_status);
                    } else if(menu_position == 0b00000010) {
                        machine_state = SETPOINT_STATE;
                        display_setpoint(temp_setpoint, new_temp_setpoint);
                    } else if(menu_position == 0b00000100) {
                        machine_state = REGISTER_STATE;
                        display_register(register_status);
                    } else if(menu_position == 0b00001000) {
                        machine_state = CONTROLLER_STATE;
                        display_controller(control_status);
                    } else if(menu_position == 0b00010000) {
                        machine_state = INSTRUCTIONS_STATE;
                        display_instructions();
                    } else if(menu_position == 0b00100000) {
                        machine_state = ABOUT_STATE;
                        display_about();
                    }
                    break;
                } else if(gpio_num == BTN1_PIN) {
                    if(menu_position > 0b00000001) {
                        menu_position >>= 1;
                    } else {
                        menu_position = 0b00100000;
                    }
                } else if(gpio_num == BTN4_PIN) {
                    if(menu_position < 0b00100000) {
                        menu_position <<= 1;
                    } else {
                        menu_position = 0b00000001;
                    }
                } else {
                    break;
                }
                display_menu(menu_position);
                break;
            
            case MONITOR_STATE:
                if(gpio_num == BTN2_PIN) {
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                }
                break;

            case SETPOINT_STATE:
                if(gpio_num == BTN2_PIN) {
                    new_temp_setpoint = temp_setpoint;
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                    break;
                } else if(gpio_num == BTN1_PIN) {
                    new_temp_setpoint += 1.0;
                } else if(gpio_num == BTN4_PIN) {
                    new_temp_setpoint -= 1.0;
                } else if(gpio_num == BTN3_PIN) {
                    temp_setpoint = new_temp_setpoint;
                    nvs_change_temp_setpoint(temp_setpoint);
                } else {
                    break;
                }
                display_setpoint(temp_setpoint, new_temp_setpoint);
                break;

            case REGISTER_STATE:
                if(gpio_num == BTN2_PIN) {
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                } else if(gpio_num == BTN3_PIN) {
                    register_status = !register_status;
                    nvs_change_register_status(register_status);
                    display_register(register_status);
                }
                break;

            case CONTROLLER_STATE:
                if(gpio_num == BTN2_PIN) {
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                } else if(gpio_num == BTN3_PIN) {
                    control_status = !control_status;
                    nvs_change_control_status(control_status);
                    display_controller(control_status);
                }
                break;

            case INSTRUCTIONS_STATE:
                if(gpio_num == BTN2_PIN) {
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                }
                break;

            case ABOUT_STATE:
                if(gpio_num == BTN2_PIN) {
                    machine_state = MENU_STATE;
                    display_clear();
                    display_menu(menu_position);
                }
                break;

            default: // Caso o estado da máquina seja inválido, exibe uma mensagem de erro e retorna a um estado seguro.
                ESP_LOGE(TAG, "Erro na máquina de estado, retornando ao menu.");
                machine_state = MENU_STATE;
                menu_position = 0b00000001;
                display_clear();
                display_menu(menu_position);
                break;
            }
        }
    }
}