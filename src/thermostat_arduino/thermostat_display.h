#include "common.h"
#include "thermostat_arduino.h"

int read_LCD_buttons();
int get_button_press();
String get_response();
void send_request(String request);
void setup_display();
void handle_main_menu();
void handle_set_desired_temperature();
void handle_set_thermostat();
void handle_display();


