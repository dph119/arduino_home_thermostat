#include <SoftwareSerial.h>
#include <math.h>
#include <Stepper.h>
#include "thermostat_arduino.h"
#include "thermostat_display.h"

const bool debug = false;
bool enabled;

// --- Stepper motor parameters
// Figure out what direction the stepper motor should turn.
// This is to provide flexibility when it comes to how the motor
// is mounted (just re-assign the pin insead of re-uploading code)
bool warmer_rotate_dir_ccw;

// Eye-balling it: 3 steps -> 2 degree change
// So 1.5 step/degree.
// Thermostat range is 40-90
// Gives us 90-40 / 1.5 = 50/1.5 = 33.3 available steps
// Only doing full steps here, so we can round up in practice
const unsigned rotate_dir_pin = 1;
const unsigned coil_a_enable_pin = 16;
const unsigned coil_b_enable_pin = 17;
const unsigned max_temp_f = 90;
const unsigned min_temp_f = 40;
const float num_step_per_degree = 2;
const unsigned num_available_steps = (unsigned)ceil((max_temp_f - min_temp_f) / num_step_per_degree);
const int steps_per_revolution = 200;  

unsigned requested_temperature;
const unsigned init_reset_temp = 65;
const unsigned min_step_delta = 5;

Stepper myStepper(steps_per_revolution, 2, 3, 11, 12);

// --- ESP Parameters
SoftwareSerial espSerial(6, 7); // RX, TX
long time_of_last_query;
// BOZO: Consider making this adjustable 
const double query_interval = 90000; // mS

// --- Temperature Parameters (from info provided via ESP)
const byte num_temperatures_to_keep = 5;
int temperature_history[num_temperatures_to_keep];
unsigned current_temperature_index;
unsigned temperature_running_average;

// --- Thermostat Parameters
long time_of_last_adjustment; 
const double default_adjustment_interval = 1800000; // mS
// BOZO: Consider making this adjustable 
double adjustment_interval_after_making_warmer;
double adjustment_interval_after_making_cooler;
double adjustment_interval;
const unsigned temperature_window = 2;
bool made_cooler_on_last_adjustment;

// --- Display parameters
SoftwareSerial dispSerial(4, 5); // RX, TX

typedef enum SerialConnections {
  hw,
  esp,
  disp
};

void setup() {
  delay(1000);
  Serial.begin(115200);
  myStepper.setSpeed(10);

 // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  enabled = true;
  reset_thermostat(false);
  set_thermostat(init_reset_temp, false);

  // It warms up faster than leaving it alone to cool off,
  // so if we just turned up the temperature, we presumably are triggering
  // the heat to turn on, so we should check more regularly to see if
  // we need to turn the thermostat down in-case we over did it.
  adjustment_interval_after_making_warmer = default_adjustment_interval / 2;
  adjustment_interval_after_making_cooler = default_adjustment_interval;
  adjustment_interval = default_adjustment_interval;
  made_cooler_on_last_adjustment = false;
  
  current_desired_temperature = init_reset_temp;
  current_temperature_index = 0;
  memset(temperature_history, -1, sizeof(temperature_history));
  query = "";
  time_of_last_query = millis();
  time_of_last_adjustment = millis();

  current_temperature = get_temperature();

  setup_display();
  
  if (debug) Serial.print("[SETUP] Setup complete!\n");
}

void regulate() {
  // Get current average temperature
  float temperature_sum = 0;
  int num_valid_temperature;
  for (int index = 0; index < num_temperatures_to_keep; ++index ) {
      if (temperature_history[index] > 0) {
          temperature_sum += temperature_history[index];
          num_valid_temperature++;
      }
  }
  temperature_running_average = temperature_sum / num_valid_temperature;

  if (debug) {
    Serial.print("[DEBUG] Running average temp: ");
    Serial.print(temperature_running_average, DEC);
    Serial.print(". num_valid_temperatures: ");
    Serial.println(num_valid_temperature, DEC);
  }

  // If the average is above the desired temperature, then adjust the thermostat
  // But by how much? Presumably the thermostat is also set to the desired temperature.
  // First we will define a window -- only regulate if the measured temperature 
  // is +/- 2 degrees of the desired.
  // Then, once we hit this condition, we adjust based on that delta.
  // For example, if the desired temperature is 70, but we measured 72, then 
  // we will turn the thermostat down by 2 degrees.
  // Of course, we need to give some time for heat to dissipate in this case.
  if (millis() - time_of_last_adjustment > adjustment_interval) {
    // Only make adjustments once we have a full window.
    // The big concern is if we no longer get valid temperatures, then
    // we better leave everything alone.
    if ( num_valid_temperature >= num_temperatures_to_keep ) {
       unsigned temperature_delta = abs(temperature_running_average - current_desired_temperature);
       if (temperature_delta > temperature_window) {
          if (temperature_running_average > current_desired_temperature) {
            // Make COLDER
            // Only do so if last adjustment was to make everything warmer.
            // The thermostat only triggers heat. It's possible we've reached an ambient
            // temperature that is simply above the desired temp. No amount of turning 
            // down the thermostat will change this, and only make it take longer
            // to turn it back up enough to turn on the heat when we want it.            
            if (!made_cooler_on_last_adjustment) {
              set_thermostat(current_thermostat_temperature - temperature_delta, true);
              made_cooler_on_last_adjustment = true;
              adjustment_interval = adjustment_interval_after_making_cooler;
            }
          }
          else {
            // Make WARMER
            set_thermostat(current_thermostat_temperature + temperature_delta, true);
            made_cooler_on_last_adjustment = false;  
            adjustment_interval = adjustment_interval_after_making_warmer;
          }
          time_of_last_adjustment = millis();
      }
    }
  }
}

void loop() {
  handle_display();
  int serial_source = hw;
  
  if ( Serial.available() ) {
      query = Serial.readStringUntil('\n');
      serial_source = esp;
  } else if ( dispSerial.available() ) {
      serial_source = disp;
  }

  if ((millis() - time_of_last_query) > query_interval) {
    if (debug) Serial.println("[AUTO] Getting temperature...");
    current_temperature = get_temperature();
    
    if (debug) {
      Serial.print("[AUTO] Received temperature: ");
      Serial.println(current_temperature, DEC);
    }
    temperature_history[current_temperature_index] = current_temperature;
    current_temperature_index++;
    if (current_temperature_index == num_temperatures_to_keep) current_temperature_index = 0;
    
    if (enabled) regulate();
    time_of_last_query = millis();
  }

  if ( query != "") {
    if ( query == "temp?" ) {
      send_ack( serial_source );
      
      if (debug) Serial.println("[CMD] Getting temperature...");
      current_temperature = get_temperature();

      if ( serial_source == disp ) {
        dispSerial.print(current_temperature, DEC);
        dispSerial.print('\n');
      }
      
      if ( serial_source == hw ) {
        Serial.print(current_temperature, DEC);
        Serial.print('\n');
      }
      
      if (debug) Serial.print("[CMD] Received temperature: ");
      if (debug) Serial.println(current_temperature, DEC);
    } else if (query == "dtemp?") {
      send_ack( serial_source );
      send_to_esp(current_desired_temperature);
    } else if (query == "enabled?") {
      send_ack( serial_source );
      send_to_esp(enabled);    
    } else if (query == "therm?") {
      send_ack( serial_source );
      send_to_esp(current_thermostat_temperature);
    } else if ( query == "rtherm!" ) {
      send_ack( serial_source );
      
      if (debug) Serial.println("[CMD] Resetting thermostat...");
      if (enabled) reset_thermostat(false);
      
      send_to_esp(current_thermostat_temperature);      
      
      if (debug) Serial.println("[CMD] Done resetting thermostat!");
    } else if ( query == "sdtemp!" ) {
      send_ack( serial_source );
      
      if (debug) Serial.println("[CMD] Setting desired temperature...");

      if ( serial_source == esp ) {
        while ( !Serial.available() );
        int value = Serial.readStringUntil('\n').toInt();
        send_ack( serial_source );
        
        // Ignore values of -1. Just return the current value.
        // This is an underhanded way of querying for the current value.
        current_desired_temperature = value > 0 ? value : current_desired_temperature;
      } 
      else {
        while ( !Serial.available() );
        send_ack( serial_source );
        current_desired_temperature = Serial.readStringUntil('\n').toInt();
      }

      if ( serial_source == esp ) send_to_esp(current_desired_temperature);
      
      if (debug) Serial.println("[CMD] Done setting desired temperature!");
      
    } else if ( query == "stherm!" ) {
      send_ack( serial_source );
      
      if (debug) Serial.println("[CMD] Waiting for requested temperature to be specified...");

      if ( serial_source == esp ) {
        while ( !Serial.available() );
        int value = Serial.readStringUntil('\n').toInt();
        send_ack( serial_source );
        
        // Ignore values of -1. Just return the current value.
        // This is an underhanded way of querying for the current value.
        requested_temperature = value > 0 ? value : current_thermostat_temperature;
        if (enabled) set_thermostat(requested_temperature, true);
      }
      else {
        while ( !Serial.available() );
        requested_temperature = Serial.readStringUntil('\n').toInt();
        send_ack( serial_source );
        if (enabled) set_thermostat(requested_temperature, true);
      }

      if ( serial_source == esp ) send_to_esp(current_thermostat_temperature);

      if (debug) Serial.println("[CMD] Done setting thermostat!");
    } 
    else if (query == "enable!") {
      send_ack( serial_source );
      enabled = true;
      if ( serial_source == esp ) {
        send_to_esp(enabled);
      }
    }
    else if (query == "disable!") {
      send_ack( serial_source );
      enabled = false;
      if ( serial_source == esp ) {
        send_to_esp(enabled);
      }
    }
    else {
      if (debug) {
        Serial.print("[CMD] Unknown command: ");
        Serial.println(query);
      }
      send_nack( serial_source );
    }
    query = "";
  } 
}

void send_to_esp(int value) {
  Serial.print(value, DEC);
  Serial.print('\n');
  Serial.flush();
}

void send_ack( int connection ) {
  if ( connection == esp ) {
    send_esp_ack();
  } else if ( connection == disp ) {
    send_disp_ack();
  }
}

void send_esp_ack() {
  Serial.print("a");
  Serial.print('\n');
  Serial.flush();  
}

void send_disp_ack() {
  dispSerial.print("a");
  dispSerial.print('\n');
  dispSerial.flush();
}

void send_nack( int connection ) {
  if ( connection == esp ) {
    send_esp_nack();
  } else if ( connection == disp ) {
    send_disp_nack();
  }
}

void send_esp_nack() {
  Serial.print("n");
  Serial.print('\n');
  Serial.flush();    
}

void send_disp_nack() {
  dispSerial.print("n");
  dispSerial.print('\n');
  dispSerial.flush();
}

void enable_stepper_motor() {
  // Add some delay to ensure everything is properly enabled before subsequent calls
  // are made to actually make the motor do something
  delay(100);
  digitalWrite(coil_a_enable_pin, HIGH);  
  digitalWrite(coil_b_enable_pin, HIGH);
}

void disable_stepper_motor() {
  digitalWrite(coil_a_enable_pin, LOW);  
  digitalWrite(coil_b_enable_pin, LOW);
}

// Use the almighty stepper motor to reset the thermostat to ... the minimum temperature
void reset_thermostat(bool use_current_temp) {
  // Worst case the thermostat is turned to the highest possible temperature
  // For now, the quick-and-dirty solution is to rotate a "sure amount" that guarantees
  // we've truly reached the "end"
  // Later we can fine-tune it for the sake of saving time
  warmer_rotate_dir_ccw = analogRead(rotate_dir_pin) > 512;
  enable_stepper_motor();
  int step_amount = use_current_temp ? degrees_to_steps(abs(current_thermostat_temperature - min_temp_f) * 1.5 ) : 4*num_available_steps;
  if (warmer_rotate_dir_ccw)
      myStepper.step(step_amount);
  else 
      myStepper.step(-1 * step_amount);
  
  disable_stepper_motor();
  current_thermostat_temperature = min_temp_f;
}

int degrees_to_steps( unsigned temperature ) {
  return (int) round(num_step_per_degree * temperature);
}

// Use the stepper motor to adjust the thermostat to the requested temperature
void set_thermostat( unsigned temperature, bool reset ) {
  // There's two parts: (1) how much to turn, and (2) in which direction.
  // How much to turn isn't much -- just abs value between the temperatures
  // Direction depends on the current temperature, and also which direction
  // the motor is facing (handled via "warmer_rotate_dir_ccw")

  // Cap temperature to min/max levels
  temperature = min(temperature, max_temp_f);
  temperature = max(temperature, min_temp_f);

  unsigned temperature_delta = abs(current_thermostat_temperature - temperature);
  bool raising_temperature = temperature > current_thermostat_temperature;

  int num_steps = degrees_to_steps(temperature_delta);
  warmer_rotate_dir_ccw = analogRead(rotate_dir_pin) > 512;

  if (debug) {
      Serial.print("[DEBUG] Temperature delta: ");
      Serial.print(temperature_delta, DEC);
      Serial.print(". num_steps: ");
      Serial.print(num_steps, DEC);
      Serial.print(". warmer_rotate_dir_ccw: ");
      Serial.print(warmer_rotate_dir_ccw, DEC);
  }
  
  int polarity = 1;
  if (raising_temperature) {
      if (warmer_rotate_dir_ccw) {
          polarity = -1;
      } 
  } 
  else {
      if (!warmer_rotate_dir_ccw) {
          polarity = -1;
      }
  }

  enable_stepper_motor();
  myStepper.step(polarity * num_steps);
  disable_stepper_motor();
  current_thermostat_temperature = temperature;

  if (debug) {
      Serial.print("[DEBUG] Current thermostat temperature: ");
      Serial.println(current_thermostat_temperature, DEC);
  }
}

// Eventually, this will be responsible for handling which CLIENT we want 
// to query for a temperature. But right now there's only 1 lone client.
// Note the magical i/f is the ESP8266, which is responsible for communicating
// with other clients (via WiFi)
int get_temperature() {
  String received_temp;
  long start_time = millis();

  Serial.print("temp?");
  Serial.print('\n');
  Serial.flush();  

   while ( !Serial.available() && (millis() - start_time) < timeout);
  // In-case we timeout, use some bogus value so we don't hang
  // It's up to the client to handle this garbage
  if ((millis() - start_time) < timeout) 
    received_temp = Serial.readStringUntil('\n');
  else received_temp = "-1";

  return received_temp.toInt();
}

