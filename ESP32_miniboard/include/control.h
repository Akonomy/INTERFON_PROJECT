#pragma once
#include <Arduino.h>

void setupPin(uint8_t pin, bool output, bool initialState = LOW);
void writePin(uint8_t pin, bool state);
bool readPin(uint8_t pin);
void blinkPin(uint8_t pin, int times = 3, int delayMs = 200);
void pulsePin(uint8_t pin, int durationMs = 500);
