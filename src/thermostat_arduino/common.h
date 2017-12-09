#ifndef COMMON_HEADER
#define COMMON_HEADER

// --- I/O Parameters
const unsigned timeout = 1000; // mS
const byte numChars = 8;
char query_char_buffer[numChars]; // an array to store the received data
String query;

// --- Common variables
int current_thermostat_temperature;
int current_temperature;
int current_desired_temperature;

#endif
