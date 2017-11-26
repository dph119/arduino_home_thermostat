#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

const bool debug = false;

// --- WiFi Client/Station Configuration
const char *ssid = "ESP8266";
const char *password = "ESP8266Test";

// --- WiFi Access Point Configuration
const char *home_ssid = "ESP8266_Home";
const char *home_password = "ESP8266_HomePass";
// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);
IPAddress    apIP(42, 42, 42, 42);  // Defining a static IP address: local & gateway
                                    // Default IP in AP mode is 192.168.4.1

// --- I/O Paramters
// Need a relatively longer timeout to give the stepper motor time to do its thing
// (for requests that involve it at least)
const unsigned timeout = 10000; // mS
const byte numChars = 8;
char data_buffer[numChars]; // an array to store the received data
String query;
String response;
const unsigned max_attempts = 10;

void handleRoot() {
    digitalWrite (LED_BUILTIN, 0); 

    // Simply display all the known state.
    
    // For now, directly query for everything.
    // The more stable solution would be to just force us to poll every x minutes
    // or something and save the results so we don't have to worry about
    // too many requests happening at any given time.
    send_request("dtemp?");
    int desired_temperature = get_response().toInt();
    delay(50);
    
    send_request("therm?");
    int current_thermostat_temperature = get_response().toInt();
    delay(50);
    
    send_request("enabled?");
    bool enabled = get_response().toInt();
    delay(50);
    
    int current_temperature = get_temperature();
    delay(50);
    
    char html[1000];
    snprintf(html, 1000, 
    "Enabled: %d<br>"
    "Current Temperature (F): %d<br>"
    "Desired Temperature (F): %d<br>"
    "Current Thermostat Temperature (F): %d<br>", 
    enabled, current_temperature, desired_temperature, current_thermostat_temperature);
    server.send(200, "text/html", html);
}

void handleEnable() {
   digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  send_request("enable!");
  response = get_response();
  serve_simple_page(response);
  
  digitalWrite ( LED_BUILTIN, 1 ); 
}

void handleDisable() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  send_request("disable!");
  response = get_response();
  serve_simple_page(response);
  
  digitalWrite ( LED_BUILTIN, 1 );
}

void handleRThermReq() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  send_request("rtherm!");
  response = get_response();
  serve_simple_page(response);
  
  digitalWrite ( LED_BUILTIN, 1 );
}

void handleThermReq() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  int temp_request = -1;
  if (server.arg("value") != "") {
      temp_request = server.arg("value").toInt();
  }

  send_request("stherm!");
  send_request(temp_request);
  response = get_response();
  serve_simple_page(response);
  
  digitalWrite ( LED_BUILTIN, 1 );
}

void handleDesiredTempReq() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on 

  int temp_request = -1;
  if (server.arg("value") != "") {
      temp_request = server.arg("value").toInt();
  }
  
  // Send request to board to update desired temperature
  send_request("sdtemp!"); 
  send_request(temp_request);
  response = get_response();
  serve_simple_page(response);
  
  digitalWrite ( LED_BUILTIN, 1 );
}

void serve_simple_page(String data) {
  // construct our reply and send it out
  char html[1000];
  data.toCharArray(data_buffer, numChars);
  
  snprintf(html, 1000, "%d", atoi( data_buffer));
  server.send(200, "text/html", html);    
}

String get_response() {
    String received_value = "-1";
    long deadline = millis() + timeout;
    while (!Serial.available() && millis() < deadline);
    // In-case we timeout, use some bogus value so we don't hang
    // It's up to the client to handle this garbage
    if (millis() < deadline) 
      received_value = Serial.readStringUntil('\n');

    return received_value;    
}

void send_request(String request) {
    // Clients are expected to provide an ack.
    // If we don't get one, re-send the request.
    // It's possible the previous request was corrupted or never went through
    String ack = "n";
    unsigned attempts = 0;
    
    // Make sure the buffer is cleared before we start
    while (Serial.available()) Serial.read();
    
    while (ack != "a" && attempts++ < max_attempts) {
      Serial.print(request);
      Serial.print('\n');
      Serial.flush();
      ack = get_response();
    }  
}

void get_ack() {
    String ack = "n";
    long deadline = millis() + timeout;
    while (!Serial.available() && millis() < deadline);
    
    // In-case we timeout, use some bogus value so we don't hang
    // It's up to the client to handle this garbage
    if (millis() < deadline) 
      received_value = Serial.read('\n');

    return received_value;  
}

void send_request(int request) {
    while ( Serial.available() ) Serial.read();
    Serial.print(request, DEC);
    Serial.print('\n');
    Serial.flush();  
}

int get_thermostat_temperature() {
    // Send request to board to update desired temperature
    send_request("therm?");
    return get_response().toInt();
}

int get_desired_temperature() {
    send_request("dtemp?");
    return get_response().toInt();  
}

int get_temperature() {
    int temperature = -1;    
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // configure traged server and url
        http.begin("http://43.43.43.43/"); //HTTP
        
        // start connection and send HTTP header
        int httpCode = http.GET();

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            if (debug)
                Serial.printf("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                temperature = payload.toInt();
            }
        } else {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
    else {
      Serial.println("[HTTP] GET Failed... Not connected.");
    }

    return temperature;
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
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
  WiFi.softAP(home_ssid, home_password);
  
  WiFi.begin(ssid, password);
  
  if (debug) Serial.print("[SETUP] Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (debug) Serial.print(".");
  }

  if (debug) {
    Serial.println();
    Serial.print("[SETUP] Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }

  server.on ( "/", handleRoot );
  server.on ( "/sdtemp", handleDesiredTempReq );
  server.on ( "/stherm", handleThermReq );
  server.on ( "/rtherm", handleRThermReq );
  server.on ( "/enable", handleEnable );
  server.on ( "/disable", handleDisable);
  server.onNotFound ( handleNotFound );
  
  server.begin();
}

void loop() {
  server.handleClient();
  
  if (Serial.available())
      query = Serial.readStringUntil('\n');

  int temperature;
  if (query != "") {
    if ( query == "temp?" ) {
        if (debug) Serial.println("[CMD] Getting temperature...");
        temperature = get_temperature();
        if (debug) Serial.print("[CMD] Received temperature: ");
        Serial.print(temperature, DEC);
        Serial.print('\n');
        Serial.flush();
    } else {
      if (debug) Serial.println("Unknown command!");
    }
    query = "";
  }
}
