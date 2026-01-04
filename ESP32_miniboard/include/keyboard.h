// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "oled.h"
#include "oled_queue.h"
#include "log.h"

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
    bool isLongPress;
} KeyEvent;



#define KEY_HOLD_TIMEOUT 456  // ms



uint32_t KEYBOARD_READ_NUMBER(uint8_t maxDigits=9, uint8_t time_scan=5  );




// === Initialization ===
void KEYBOARD_INIT(); // Call once during setup

// === Basic Key Reading ===
// Reads a key, waits for release, debounced
KeyEvent KEYBOARD_READ_KEY(uint8_t mode=0);



void keypadMultiTapTest(void);

char* KEYBOARD_READ_STRING(void);


char* KEYBOARD_READ(uint8_t mode=0,uint8_t special_key=0);


char KEYBOARD_READ_CONTROL(uint8_t timeoutSeconds=5);



void TESTkeypadScan(void);
void TEST_KEYBOARD_STATE(void);


bool KEYBOARD_ACTIVE(uint8_t seconds=10);

#endif // KEYBOARD_H
