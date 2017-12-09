#include <SFE_BMP180.h>
#include <Wire.h>

SFE_BMP180 temperature;
String query;

void setup() {
  delay(1000);
  Serial.begin(115200);

 // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // Check if we're "connected" to the temperature sensor
  if ( !temperature.begin() ) {
    while( true ){
      // Blink LED in unique way to try and ease HW debug...
      // Two quick blinks (in 1 second), followed by pause with LED off.
      digitalWrite(LED_BUILTIN, HIGH);  
      delay(250);                       
      digitalWrite(LED_BUILTIN, LOW);    
      delay(250); 
      digitalWrite(LED_BUILTIN, HIGH);  
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);    
      delay(250); 
      delay(1000);
    }
  }
}

void loop() {
  while ( !Serial.available() );
  query = Serial.readStringUntil('\n');

  if ( query == "temp?" ) {
      Serial.print(get_temperature(), DEC);
      Serial.print('\n');
      Serial.flush();
  } else {
    Serial.println("Unknown command!");
  }
}

// Implicit cast to int from double.
// Frankly idgaf if I don't have the temperature down to multiple decimal places
int get_temperature() {
  char status;
  double temp_c,temp_f, P,p0,a;

  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.
  temp_f = -1;
  
  status = temperature.startTemperature();
  if (status != 0) {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    status = temperature.getTemperature(temp_c);
    if (status != 0) {
      temp_f = (9.0 / 5.0) * temp_c + 32.0;
     }
  }

  return temp_f;
}

