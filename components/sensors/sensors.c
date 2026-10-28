#include <stdio.h>
#include "sensors.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "dht.h"


// Manuseador para o ADC do sensor LDR.
static adc_oneshot_unit_handle_t adc1_handle;


// Configura o ADC para leitura do sensor LDR.
void sensors_config_init(adc_unit_t ldr_unit, adc_channel_t ldr_channel) { 
    // Variável de configuração da unidade do ADC.
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ldr_unit
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle)); // Cria a nova unidade e verifica se houve erros.

    // Variável de configuração do canal do ADC.
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ldr_channel, &config)); // Configura o canal e verifica se houve erros.
}

// Função para realizar a leitura dos sensores.
void sensors_read_value(int *raw_value, adc_channel_t ldr_channel, float *humid, float *temp, gpio_num_t dht_pin) { 
    adc_oneshot_read(adc1_handle, ldr_channel, raw_value);
    dht_read_float_data(DHT_TYPE_DHT11, dht_pin, humid, temp);
}