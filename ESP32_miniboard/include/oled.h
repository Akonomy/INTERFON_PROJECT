#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===========================
// OLED CONFIGURATION
// ===========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ===========================
// DEFAULT PARAMETERS
// ===========================
#define DEFAULT_TEXT_SIZE 2
#define DEFAULT_TEXT_X 0
#define DEFAULT_TEXT_Y 0
#define PASSWORD_SHOW_TIME 150  // ms before changing char to '*'

// ===========================
// PUBLIC FUNCTIONS
// ===========================

// Must be called once in setup()
void OLED_Init();

// Clears the display
void OLED_Clear();

// Displays a number centered or at default position
void OLED_DisplayNumber(uint8_t number);

// Displays arbitrary text at default position
void OLED_DisplayText(const String &text, uint8_t size = 2);


// Displays debug text with custom position, size, and optional clearing
void OLED_DisplayDebug(const String &text, int x = 0, int y = 0, int size = 1, bool clear = true);

// Displays a password input (shows last char briefly before replacing with '*')
void OLED_DisplayPassword(const String &input, bool reset = false);

// Updates internal timers (call this in loop)
void OLED_Update();


void OLED_DisplayStrictText(const String &line1, const String &line2);
#endif
