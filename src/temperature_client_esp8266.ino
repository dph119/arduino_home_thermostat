#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

#define TIMEOUT 5000 // mS

ESP8266WiFiMulti WiFiMulti;

IPAddress    apIP(43, 43, 43, 43);  // Defining a static IP address: local & gateway
                                    // Default IP in AP mode is 192.168.4.1

const byte numChars = 8;
char received_temp_char_buffer[numChars]; // an array to store the received data
String received_temp;

const char *ssid = "ESP8266";
const char *password = "ESP8266Test";

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

void handleRoot() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  // query the board for the temperature
  while ( Serial.available() ) Serial.read();
  Serial.print("temp?");
  Serial.print('\n');
  Serial.flush();
  long deadline = millis() + TIMEOUT;
  while ( !Serial.available() && millis() < deadline );
  // In-case we timeout, use some bogus value so we don't hang
  // It's up to the client to handle this garbage
  if (millis() < deadline) 
    received_temp = Serial.readStringUntil('\n');
  else received_temp = "-1";
  
  // construct our reply and send it out
  char html[1000];
  received_temp.toCharArray( received_temp_char_buffer, numChars );
  
  snprintf ( html, 1000, "%d", atoi( received_temp_char_buffer ) );
  server.send ( 200, "text/html", html );
  digitalWrite ( LED_BUILTIN, 1 );
}

void handleNotFound() {
  digitalWrite ( LED_BUILTIN, 0 );
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  digitalWrite ( LED_BUILTIN, 1 ); //turn the built in LED on pin DO of NodeMCU off
}

void setup() {    
    Serial.begin(115200);

    for(uint8_t t = 4; t > 0; t--) {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    Serial.printf("[SETUP] Setting mode...\n");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
  
    Serial.printf("[SETUP] Adding access point...\n");

    WiFiMulti.addAP(ssid, password);

    server.on ( "/", handleRoot );
    server.onNotFound ( handleNotFound );
  
    server.begin();
    Serial.printf("[SETUP] Done!\n");
}

void loop() {
  server.handleClient();
}







