#ifndef REGISTER_H
#define REGISTER_H

#include <stdbool.h>

void register_init();

void nvs_load_last_config(float *temp_setpoint, bool *register_status, bool *control_status);

void nvs_change_temp_setpoint(float temp_setpoint);

void nvs_change_register_status(bool register_status);

void nvs_change_control_status(bool control_status);

void logger_write(float temp, float humid, int bright);

void logger_read();
#endif