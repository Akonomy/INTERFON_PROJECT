#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>
#include "gpio.h"

// ===========================
// CONFIGURATION
// ===========================

// Default update rate (ms)
#define KEYBOARD_UPDATE_RATE_MS 25

// Analog threshold for detecting key press
#define KEYBOARD_HIGH_THRESHOLD 2500

// Keypad dimensions
#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 3

// ===========================
// PUBLIC API
// ===========================

void KEYBOARD_Init();                 // Initialize the keypad module
void KEYBOARD_Update();               // Call periodically (e.g., in loop)
bool KEYBOARD_HasNewKey();            // Returns true if a new key was pressed
char KEYBOARD_GetLastKey();           // Returns last pressed key (clears flag)
char KEYBOARD_PeekLastKey();          // Returns last pressed key (without clearing)
void KEYBOARD_SetUpdateRate(uint16_t rateMs);  // Change update interval
void KEYBOARD_SetThreshold(uint16_t threshold); // Change analog threshold

#endif
