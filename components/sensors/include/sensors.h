#ifndef SENSORS_H
#define SENSORS_H

#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

void sensors_config_init(adc_unit_t ldr_unit, adc_channel_t ldr_channel);

void sensors_read_value(int *raw_value, adc_channel_t ldr_channel, float *humid_dht, float *temp_dht, gpio_num_t dht_pin);

#endif
