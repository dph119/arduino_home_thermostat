#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

const bool debug = false;

// --- I/O Parameters
const byte numChars = 8;
char data_buffer[numChars]; // an array to store the received data
String response;
const int timeout = 5000; // mS

// --- WiFi Parameters
const char *ssid = "ESP8266";
const char *password = "ESP8266Test";
IPAddress    apIP(43, 43, 43, 43);  // Defining a static IP address: local & gateway
                                    // Default IP in AP mode is 192.168.4.1
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);

void send_request(String command) {
  while ( Serial.available() ) Serial.read();
  Serial.print(command);
  Serial.print('\n');
  Serial.flush();  
}

String get_response() {
  String response;
  
  long deadline = millis() + timeout;
  while (!Serial.available() && millis() < deadline);
  // In-case we timeout, use some bogus value so we don't hang
  // It's up to the client to handle this garbage
  if (millis() < deadline) 
    response = Serial.readStringUntil('\n');
  else response = "-1";

  return response;
}

void serve_simple_page(String data) {
  // construct our reply and send it out
  char html[1000];
  data.toCharArray(data_buffer, numChars);
  
  snprintf(html, 1000, "%d", atoi( data_buffer));
  server.send(200, "text/html", html);    
}

void handleRoot() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  send_request("temp?");
  response = get_response();
  serve_simple_page(response);

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

  if (debug) {
    for(uint8_t t = 4; t > 0; t--) {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }
    Serial.printf("[SETUP] Setting mode...\n");
  }
    
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  

  if (debug) Serial.printf("[SETUP] Adding access point...\n");

  WiFiMulti.addAP(ssid, password);

  server.on ( "/", handleRoot );
  server.onNotFound ( handleNotFound );

  server.begin();
  if (debug) Serial.printf("[SETUP] Done!\n");
}

void loop() {
  server.handleClient();
}







