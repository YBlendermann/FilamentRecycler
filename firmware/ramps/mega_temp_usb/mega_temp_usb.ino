/* FilamentRecycler Temperaturtest V0.2
 * Mega 2560 + RAMPS 1.6
 * Messung aus Original V0.4.1 beibehalten; keinerlei Heiz-/Motor-Ausgaenge.
 * Protokoll: HELLO, PING, GET_TEMP -> ID..., PONG, TEMP,...
 */
#include <Arduino.h>
#include <math.h>

const uint8_t TEMP_PIN[3] = {A13, A15, A14}; // Eingang, Mitte, Duese
const float R_NOMINAL = 100000.0f;
const float T_NOMINAL = 25.0f;
const float BETA = 3950.0f;
const float R_PULLUP = 4700.0f;
const unsigned long SENSOR_INTERVAL = 500;
float temp[3] = {NAN, NAN, NAN};
unsigned long lastSample = 0;
char input[48];
uint8_t inputLen = 0;

float readTemperature(uint8_t pin) {
  analogRead(pin); // ersten Wert nach Kanalwechsel verwerfen
  unsigned long sum = 0;
  for (uint8_t i = 0; i < 10; ++i) sum += analogRead(pin);
  float adc = sum / 10.0f;
  if (adc <= 2 || adc >= 1021) return NAN;
  float r = R_PULLUP * adc / (1023.0f - adc);
  float invT = 1.0f / (T_NOMINAL + 273.15f) + log(r / R_NOMINAL) / BETA;
  float t = 1.0f / invT - 273.15f;
  return (isfinite(t) && t >= -20 && t <= 300) ? t : NAN;
}

void sampleSensors() {
  for (uint8_t i = 0; i < 3; ++i) temp[i] = readTemperature(TEMP_PIN[i]);
}

void sendTemps() {
  Serial.print(F("TEMP"));
  for (uint8_t i = 0; i < 3; ++i) {
    Serial.print(',');
    if (isfinite(temp[i])) Serial.print(temp[i], 1);
    else Serial.print(F("ERR"));
  }
  Serial.println();
}

void handleCommand(const char *command) {
  if (!strcmp(command, "HELLO")) Serial.println(F("ID,RAMPS_TEMP,0.2"));
  else if (!strcmp(command, "PING")) Serial.println(F("PONG"));
  else if (!strcmp(command, "GET_TEMP")) sendTemps();
  else Serial.println(F("ERR,UNKNOWN_COMMAND"));
}

void setup() {
  Serial.begin(115200);
  analogReference(DEFAULT);
  sampleSensors();
  lastSample = millis();
}

void loop() {
  if (millis() - lastSample >= SENSOR_INTERVAL) {
    lastSample = millis();
    sampleSensors();
  }
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      input[inputLen] = '\0';
      handleCommand(input);
      inputLen = 0;
    } else if (c != '\r') {
      if (inputLen < sizeof(input) - 1) input[inputLen++] = c;
      else inputLen = 0;
    }
  }
}
