#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include "common.h"
#include "thermostat_display.h"

SoftwareSerial hostSerial(6, 7); // RX, TX

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
 
// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Used when user is deciding on a new value
int new_desired_temperature;
int new_thermostat_temperature;
bool state_change;

int current_menu;
typedef enum MenuState {
  main_menu,
  modify_thermostat_temperature,
  modify_desired_temperature,
  num_states // always leave last -- tells us how many states there are
};

// read the buttons
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 if (adc_key_in < 50)   return btnRIGHT; 
 if (adc_key_in < 195)  return btnUP;
 if (adc_key_in < 380)  return btnDOWN;
 if (adc_key_in < 555)  return btnLEFT;
 if (adc_key_in < 790)  return btnSELECT;  
 return btnNONE;  // when all others fail, return this...
}

// BOZO: Polling sucks. Look into how to use Arduino's interrupts
int get_button_press() {
  int button_val = -1;
  int consecutive_times_nothing = 0;
  int consecutive_times_pressed = 0;
  int val;
  bool button_pressed = false;
  bool button_released = false;
  
  do {
    val = read_LCD_buttons();

    if ( val != btnNONE ) {
      consecutive_times_pressed++;

      if ( !button_pressed && consecutive_times_pressed > 5 ) {
        button_pressed = true;
        button_val = val;
      }
    } else if ( val == btnNONE && !button_pressed )
      consecutive_times_pressed = 0;
      
    // ---------------------------------------------------
    
    if ( val == btnNONE ) {
      consecutive_times_nothing++;

      if ( button_pressed && consecutive_times_nothing > 5 ) {
        button_released = true;
      }
    } else if ( val != btnNONE && button_pressed )
      consecutive_times_nothing = 0;
    
  } while ( !(button_pressed && button_released) && consecutive_times_nothing < 5 ); 

  return button_val;
}

void setup_display() {
  // We expect initial temperature information to be set/provided by the main board
  current_menu = main_menu;
  new_desired_temperature = current_desired_temperature;
  new_thermostat_temperature = current_thermostat_temperature;

  hostSerial.begin(115200);

  lcd.begin(16, 2);
  lcd.clear();
  delay(500);  
}

void handle_main_menu() {
  if (state_change) {
    lcd.clear();
    state_change = false;
  } else {
    lcd.setCursor(0,0);
  }
  
  lcd.print("Cur: "); 
  lcd.print(current_temperature);
  lcd.print(" Thrm: ");
  lcd.print(current_thermostat_temperature);
  lcd.setCursor(0,1);
  lcd.print("Des: "); 
  lcd.print(current_desired_temperature);
  lcd.print("");

  lcd_key = get_button_press();
  if (lcd_key == btnSELECT) {
    current_menu = (current_menu + 1) %  num_states;
    state_change = true;
  }
}

void handle_set_desired_temperature() {
  if (state_change) {
    lcd.clear();
    state_change = false;
  } else {
    lcd.setCursor(0,0);
  }

  lcd.print("New des temp: ");
  lcd.print(new_desired_temperature);
  // Edge case where second char is not used
  // make sure we explicitly clear it in case it was
  // used previously (e.g. 10->9, -1->0)
  if ( new_desired_temperature < 10 )
    lcd.print(' ');
  
  lcd_key = get_button_press();
  if (lcd_key == btnSELECT) {
    current_menu = (current_menu + 1) %  num_states;
    state_change = true;
    current_desired_temperature = new_desired_temperature;
  } else if (lcd_key == btnUP) {
    new_desired_temperature++;
  } else if (lcd_key == btnDOWN) {
    new_desired_temperature--;
  }
}

void handle_set_thermostat() {
  if (state_change) {
    lcd.clear();
    state_change = false;
  } else {
    lcd.setCursor(0,0);
  }

  lcd.print("New temp: ");
  lcd.print(new_thermostat_temperature);
  if ( new_desired_temperature < 10 )
    lcd.print(' ');

  lcd_key = get_button_press();
  if (lcd_key == btnSELECT) {
    current_menu = (current_menu + 1) %  num_states;
    state_change = true;
    set_thermostat(new_thermostat_temperature, false);
  } else if (lcd_key == btnUP) {
    new_thermostat_temperature++;
  } else if (lcd_key == btnDOWN) {
    new_thermostat_temperature--;
  }
}

void handle_display() {
  switch( current_menu ) {
    case main_menu: handle_main_menu(); break;
    case modify_desired_temperature: handle_set_desired_temperature(); break;
    case modify_thermostat_temperature: handle_set_thermostat(); break;
  };

  // Check for any new data sent from the host Arduino
}
