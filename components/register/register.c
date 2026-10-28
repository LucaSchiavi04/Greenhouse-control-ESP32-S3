#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "register.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_littlefs.h"
#include "sdkconfig.h"


static nvs_handle_t nvs_handler; // Manuseador do NVS.
esp_err_t err; // Variável para armazenar o código de erro das funções de registro.
const static char *TAG = "REGISTER"; // Tag para log de registros.


// Função para inicializar os sistemas de armazenamento.
void register_init() {
    // Inicializa o NVS.
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Inicializa o LittleFS.
    esp_vfs_littlefs_conf_t lfs_config = {
        .base_path = "/littlefs",
        .partition_label = "logger",
        .format_if_mount_failed = true,
        .dont_mount = false
    };

    err = esp_vfs_littlefs_register(&lfs_config);
    switch (err) {
        case ESP_FAIL:
            ESP_LOGE(TAG, "Falha ao montar ou formatar o sistema de arquivos");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(TAG, "Partição 'logger' não foi encontrada na tabela de partições");
            break;
        case ESP_OK:
            ESP_LOGI(TAG, "LittleFS montado com sucesso");

            // Toda vez que o microcontrolador for reinciado, o log.txt será reiniciado (limpo).
            FILE *f = fopen("/littlefs/log.txt", "w");
            if (f != NULL) {
                fprintf(f, "--------------- INÍCIO DO LOG DE MONITORAMENTO ---------------\n");
                fclose(f);
                ESP_LOGI(TAG, "Arquivo log.txt reinicializado para a nova sessão.");
            } else {
                ESP_LOGE(TAG, "Erro ao criar/zerar o arquivo log.txt na inicialização");
            }
            break;

        default:
            ESP_LOGE(TAG, "Falha em inicializar LittleFS (%s)", esp_err_to_name(err));
            break;
    }
}

// Função para carregar as últimas configurações salvas na NVS (SP, REG_STATUS, CTRL_STATUS).
void nvs_load_last_config(float *temp_setpoint, bool *register_status, bool *control_status) {
    err = nvs_open("storage", NVS_READWRITE, &nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o handle do NVS (%s)", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Lendo configurações armazenadas na memória...");

    // Setpoint de temperatura (float na RAM, uint8_t na NVS).
    uint8_t val_u8 = 0;
    err = nvs_get_u8(nvs_handler, "setpoint", &val_u8);
    switch (err) {
        case ESP_OK:
            *temp_setpoint = (float)val_u8; // Conversão explícita para float
            ESP_LOGI(TAG, "Setpoint = %.0f", *temp_setpoint);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "Valor não salvo ainda. Inicializando setpoint...");
            *temp_setpoint = (float)CONFIG_DEFAULT_TEMP_SETPOINT;

            err = nvs_set_u8(nvs_handler, "setpoint", (uint8_t)*temp_setpoint);
            ESP_LOGI(TAG, "Guardando setpoint: %s", (err != ESP_OK) ? "Falha" : "Feito");
            break;
        default:
            ESP_LOGE(TAG, "Erro ao realizar leitura do setpoint (%s)", esp_err_to_name(err));
    }

    // Estado de registro (booleano na RAM, uint8_t na NVS).
    err = nvs_get_u8(nvs_handler, "reg_status", &val_u8);
    switch (err) {
        case ESP_OK:
            *register_status = (val_u8 != 0); // Converte uint8_t para bool
            ESP_LOGI(TAG, "Gravação de dados: %s", *register_status ? "Ligado" : "Desligado");
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "Valor não salvo ainda. Inicializando estado de registro...");
            *register_status = CONFIG_DEFAULT_REGISTER_STATUS;

            err = nvs_set_u8(nvs_handler, "reg_status", (uint8_t)*register_status);
            ESP_LOGI(TAG, "Guardando estado de registro: %s", (err != ESP_OK) ? "Falha" : "Feito");
            break;
        default:
            ESP_LOGE(TAG, "Erro ao realizar leitura do reg_status (%s)", esp_err_to_name(err));
    }

    // Estado de controle (booleano na RAM, uint8_t na NVS).
    err = nvs_get_u8(nvs_handler, "ctrl_status", &val_u8);
    switch (err) {
        case ESP_OK:
            *control_status = (val_u8 != 0); // Converte uint8_t para bool
            ESP_LOGI(TAG, "Controle da ventoinha: %s", *control_status ? "Ligado" : "Desligado");
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "Valor não salvo ainda. Inicializando estado de controle...");
            *control_status = CONFIG_DEFAULT_TEMP_CONTROL_STATUS;

            err = nvs_set_u8(nvs_handler, "ctrl_status", (uint8_t)*control_status);
            ESP_LOGI(TAG, "Guardando estado de controle: %s", (err != ESP_OK) ? "Falha" : "Feito");
            break;
        default:
            ESP_LOGE(TAG, "Erro ao realizar leitura do ctrl_status (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao realizar o commit (%s)", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handler);
}

// Função que muda o setpoint de temperatura na NVS toda vez que ele é alterado.
void nvs_change_temp_setpoint(float temp_setpoint) {
    err = nvs_open("storage", NVS_READWRITE, &nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o handle do NVS (%s)", esp_err_to_name(err));
        return;
    }
    err = nvs_set_u8(nvs_handler, "setpoint", (uint8_t)temp_setpoint);
    ESP_LOGI(TAG, "Guardando setpoint: %s", (err != ESP_OK) ? "Falha" : "Feito");
    
    err = nvs_commit(nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao realizar o commit (%s)", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handler);
}

// Função que muda o estado de registro na NVS toda vez que ele é alterado.
void nvs_change_register_status(bool register_status) {
    err = nvs_open("storage", NVS_READWRITE, &nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o handle do NVS (%s)", esp_err_to_name(err));
        return;
    }
    err = nvs_set_u8(nvs_handler, "reg_status", (uint8_t)register_status);
    ESP_LOGI(TAG, "Guardando estado de registro: %s", (err != ESP_OK) ? "Falha" : "Feito");

    err = nvs_commit(nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao realizar o commit (%s)", esp_err_to_name(err));
    }

    nvs_close(nvs_handler);
}

// Função que muda o estado de controle na NVS toda vez que ele é alterado.
void nvs_change_control_status(bool control_status) {
    err = nvs_open("storage", NVS_READWRITE, &nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o handle do NVS (%s)", esp_err_to_name(err));
        return;
    }
    err = nvs_set_u8(nvs_handler, "ctrl_status", (uint8_t)control_status);
    ESP_LOGI(TAG, "Guardando estado de controle: %s", (err != ESP_OK) ? "Falha" : "Feito");

    err = nvs_commit(nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao realizar o commit (%s)", esp_err_to_name(err));
    }

    nvs_close(nvs_handler);
}

// Função que escreve os dados de monitoramento no arquivo log.txt.
void logger_write(float temp, float humid, int bright) {
    // Obtem-se o tempo de uptime do microcontrolador para referência temporal do log, em milissegundos.
    uint32_t uptime_ms = esp_log_timestamp();

    // Conversão para horas, minutos e segundos.
    uint32_t total_sec = uptime_ms / 1000;
    uint32_t hours = total_sec / 3600;
    uint32_t minutes = (total_sec % 3600) / 60;
    uint32_t seconds = total_sec % 60;

    FILE *f = fopen("/littlefs/log.txt", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita");
        return;
    }
    fprintf(f, "[%03lu:%02lu:%02lu] Temperatura: %.0f°C, Umidade: %.0f%%, Luminosidade: %d\n", 
            (unsigned long)hours,
            (unsigned long)minutes,
            (unsigned long)seconds,
            temp, humid, bright);
    // Exemplo de linha: [001:02:30] Temperatura: 25°C, Umidade: 50%, Luminosidade: 3000
    fclose(f);
}

// Imprime as linhas escritas no arquivo log.txt no console serial.
void logger_read() {
    FILE *f = fopen("/littlefs/log.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para leitura");
        return;
    }
    char line[128];
    printf("\nLOG:\n");
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("%s", line);
    }
    printf("\n----------------------------------------------------------------\n");
    /*
     *  Exemplo de impressão:
     *  LOG:
     * 
     *  --------------- INÍCIO DO LOG DE MONITORAMENTO ---------------
     * 
     *  [000:00:05] Temperatura: 25°C, Umidade: 50%, Luminosidade: 3000
     *  [000:00:07] Temperatura: 26°C, Umidade: 49%, Luminosidade: 3500
     *  [000:00:09] Temperatura: 26°C, Umidade: 48%, Luminosidade: 3200
     * 
     *  ----------------------------------------------------------------
     * 
     */
    fclose(f);
}
