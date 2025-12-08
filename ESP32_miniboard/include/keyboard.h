// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "oled.h"
// === Key Types ===
typedef enum {
    KEY_NONE = 0,   // No key pressed
    KEY_CHAR,       // 0-9
    KEY_ENTER,      // #
    KEY_CLEAR       // *
} KeyType;

// === Key Event Struct ===
typedef struct {
    KeyType type;   // Type of key event
    char value;     // Key value (e.g., '5', '*' or '#')
} KeyEvent;

// === Initialization ===
void KEYBOARD_INIT(); // Call once during setup

// === Basic Key Reading ===
// Reads a key, waits for release, debounced
KeyEvent KEYBOARD_READ_KEY();

// === Multi-Digit Input ===
// Read digits until '#' or maxDigits reached
int KEYBOARD_READ_NUMBER(int maxDigits);

// Get current input buffer (copy into your own buffer)
void KEYBOARD_GET_INPUT(char* buffer, int maxLen);

// Clears the input buffer (e.g., after '*')
void KEYBOARD_CLEAR_BUFFER();

// === Multi-Tap Text Input ===
// Simulates old-style SMS input (e.g., '2' = A/B/C/2)
// Waits for key press and cycles characters
char KEYBOARD_READ_CHAR();


void KEYBOARD_READ_STRING(char* buffer, int maxLen, bool stopOnEnter);


char* KEYBOARD_READ(uint8_t mode);

#endif // KEYBOARD_H
