#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

void display_config_init(int16_t sda_pin, int16_t scl_pin, int16_t reset_pin);

void display_menu(uint8_t menu_posi);

void display_monitor(int bright, float humid, float temp, float temp_setpoint, bool fan_status);

void display_setpoint(float temp_setpoint, float new_temp_setpoint);

void display_register(bool register_status);

void display_controller(bool control_status);

void display_instructions();

void display_about();

void display_clear();

#endif
