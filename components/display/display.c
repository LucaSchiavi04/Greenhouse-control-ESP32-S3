#include <stdio.h>
#include <stdbool.h>
#include "display.h"
#include "ssd1306.h"


char text_buffer[17]; // Buffer de texto para armazenar temporariamente as strings a serem exibidas no display.
SSD1306_t disp; // Estrutura de dados para armazenar as informações do display OLED SSD1306.


// Configura o display OLED SSD1306.
void display_config_init(int16_t sda_pin, int16_t scl_pin, int16_t reset_pin) { 
    i2c_master_init(&disp, sda_pin, scl_pin, reset_pin);
    ESP_ERROR_CHECK(ssd1306_init(&disp, 128, 64));
    ssd1306_clear_screen(&disp, false);
	ssd1306_contrast(&disp, 0xff);
}

// Exibe o menu principal no display.
void display_menu(uint8_t menu_posi) { 
    ssd1306_display_text(&disp, 0, "1 - Monitor     ", 16, (menu_posi & 0x01) != 0);
    ssd1306_display_text(&disp, 1, "2 - Set-Point   ", 16, (menu_posi & 0x02) != 0);
    ssd1306_display_text(&disp, 2, "3 - Register    ", 16, (menu_posi & 0x04) != 0);
    ssd1306_display_text(&disp, 3, "4 - Controller  ", 16, (menu_posi & 0x08) != 0);
    ssd1306_display_text(&disp, 4, "5 - Instructions", 16, (menu_posi & 0x10) != 0);
    ssd1306_display_text(&disp, 5, "6 - About       ", 16, (menu_posi & 0x20) != 0);
    ssd1306_display_text(&disp, 6, "        Continue", 16, false);
    ssd1306_display_text(&disp, 7, "          BT3-->", 16, false);
}

// Exibe os valores lidos dos sensores no display.
void display_monitor(int bright, float humid, float temp, float temp_setpoint, bool fan_status) { 
    sprintf(text_buffer, "Brightness: %d", bright);
    ssd1306_display_text(&disp, 0, text_buffer, 16, false);
    sprintf(text_buffer, "Humidity: %.0f%%", humid);
    ssd1306_display_text(&disp, 1, text_buffer, 13, false);
    sprintf(text_buffer, "Temp: %.0f C", temp);
    ssd1306_display_text(&disp, 2, text_buffer, 10, false);
    sprintf(text_buffer, "Temp SP: %.0f C", temp_setpoint);
    ssd1306_display_text(&disp, 3, text_buffer, 13, false);
    sprintf(text_buffer, "Fan: %s", fan_status ? "ON " : "OFF");
    ssd1306_display_text(&disp, 4, text_buffer, 8, false);
    ssd1306_display_text(&disp, 6, "Return          ", 16, false);
    ssd1306_display_text(&disp, 7, "<--BT2          ", 16, false);
}

// Exibe a tela de ajuste do setpoint de temperatura no display.
void display_setpoint(float temp_setpoint, float new_temp_setpoint) { 
    ssd1306_display_text(&disp, 0, "Temp Set-Point: ", 16, false);
    sprintf(text_buffer, " %.0f C", new_temp_setpoint);
    ssd1306_display_text_x3(&disp, 2, text_buffer, 5, false);
    sprintf(text_buffer, "Old SP: %.0f C", temp_setpoint);
    ssd1306_display_text(&disp, 5, text_buffer, 12, false);
    ssd1306_display_text(&disp, 6, "Return   Confirm", 16, false);
    ssd1306_display_text(&disp, 7, "<--BT2    BT3-->", 16, false);
}

// Exibe a tela de status do registro no display.
void display_register(bool register_status) { 
    ssd1306_display_text(&disp, 0, "Register Status:", 16, false);
    if (register_status) {
        ssd1306_display_text_x3(&disp, 2, " ON  ", 5, false);
    } else {
        ssd1306_display_text_x3(&disp, 2, " OFF ", 5, false);
    }
    ssd1306_display_text(&disp, 6, "Return    Change", 16, false);
    ssd1306_display_text(&disp, 7, "<--BT2    BT3-->", 16, false);
}

// Exibe a tela de status do controle de temperatura no display.
void display_controller(bool control_status) { 
    ssd1306_display_text(&disp, 0, "Control Status: ", 16, false);
    if (control_status) {
        ssd1306_display_text_x3(&disp, 2, " ON  ", 5, false);
    } else {
        ssd1306_display_text_x3(&disp, 2, " OFF ", 5, false);
    }
    ssd1306_display_text(&disp, 6, "Return    Change", 16, false);
    ssd1306_display_text(&disp, 7, "<--BT2    BT3-->", 16, false);
}

// Exibe a tela de instruções de uso do sistema no display.
void display_instructions() { 
    ssd1306_display_text(&disp, 0, "       ^        ", 16, false);
    ssd1306_display_text(&disp, 1, "       |        ", 16, false);
    ssd1306_display_text(&disp, 2, "      BT1       ", 16, false);
    ssd1306_display_text(&disp, 3, "<-BT2     BT3-> ", 16, false);
    ssd1306_display_text(&disp, 4, "      BT4       ", 16, false);
    ssd1306_display_text(&disp, 5, "       |        ", 16, false);
    ssd1306_display_text(&disp, 6, "       v        ", 16, false);
    ssd1306_display_text(&disp, 7, "5: Save 6: Buzz ", 16, false);
}

// Exibe a tela de informações sobre o sistema no display.
void display_about() { 
    ssd1306_display_text(&disp, 0, "About           ", 16, false);
    ssd1306_display_text(&disp, 1, "Created By:     ", 16, false);
    ssd1306_display_text(&disp, 2, "Luca Schiavi    ", 16, false);
    ssd1306_display_text(&disp, 3, "GitHub:         ", 16, false);
    ssd1306_display_text(&disp, 4, "@LucaSchiavi04  ", 16, false);
    ssd1306_display_text(&disp, 6, "Return          ", 16, false);
    ssd1306_display_text(&disp, 7, "<--BT2          ", 16, false);
}

// Limpa a tela do display.
void display_clear() { 
    ssd1306_clear_screen(&disp, false);
}