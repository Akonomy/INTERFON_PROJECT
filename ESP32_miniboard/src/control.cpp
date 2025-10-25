#include "control.h"
#include <Arduino.h>

void setupPin(uint8_t pin, bool output, bool initialState) {
  pinMode(pin, output ? OUTPUT : INPUT);
  if (output) digitalWrite(pin, initialState ? HIGH : LOW);
  Serial.printf("[CTRL] Pin %d set as %s\n", pin, output ? "OUTPUT" : "INPUT");
}

void writePin(uint8_t pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
  Serial.printf("[CTRL] Pin %d -> %s\n", pin, state ? "HIGH" : "LOW");
}

bool readPin(uint8_t pin) {
  int val = digitalRead(pin);
  Serial.printf("[CTRL] Pin %d read -> %d\n", pin, val);
  return val;
}

void blinkPin(uint8_t pin, int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    writePin(pin, HIGH);
    delay(delayMs);
    writePin(pin, LOW);
    delay(delayMs);
  }
}

void pulsePin(uint8_t pin, int durationMs) {
  writePin(pin, HIGH);
  delay(durationMs);
  writePin(pin, LOW);
}
