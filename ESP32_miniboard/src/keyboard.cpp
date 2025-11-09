// keyboard.cpp
#include "keyboard.h"
#include "gpio.h"
#include <Arduino.h>
#include <string.h>

static const uint8_t ROW_PINS[4] = { 201, 204, 203, 202 }; // R2, R4, R6, R7
static const uint8_t COL_PINS[3] = { 101, 100, 102 }; // C1, C3, C5

static const char KEYPAD[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

#define DEBOUNCE_DELAY 20
#define HOLD_THRESHOLD 400
#define MAX_INPUT_LEN 16

static char inputBuffer[MAX_INPUT_LEN + 1];
static int inputIndex = 0;
static char lastKey = '\0';

void KEYBOARD_INIT() {
    for (int i = 0; i < 4; ++i) {
        GPIO_SET(ROW_PINS[i], LOW);
    }
    memset(inputBuffer, 0, sizeof(inputBuffer));
    inputIndex = 0;
}

static bool waitForKeyRelease(char key) {
    unsigned long start = millis();
    while (millis() - start < HOLD_THRESHOLD) {
        KeyEvent ev = KEYBOARD_READ_KEY();
        if (ev.value != key)
            return true;
        delay(10);
    }
    return true;
}

KeyEvent KEYBOARD_READ_KEY() {
    for (int row = 0; row < 4; ++row) {
        GPIO_SET(ROW_PINS[row], HIGH);

        for (int col = 0; col < 3; ++col) {
            if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH) {
                delay(DEBOUNCE_DELAY);
                if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH) {
                    char key = KEYPAD[row][col];
                    GPIO_SET(ROW_PINS[row], LOW);
                    waitForKeyRelease(key);

                    KeyEvent ev;
                    ev.value = key;

                    if (key == '#') ev.type = KEY_ENTER;
                    else if (key == '*') ev.type = KEY_CLEAR;
                    else ev.type = KEY_CHAR;

                    lastKey = key;
                    return ev;
                }
            }
        }
        GPIO_SET(ROW_PINS[row], LOW);
    }
    KeyEvent none = { KEY_NONE, '\0' };
    return none;
}

int KEYBOARD_READ_NUMBER(int maxDigits) {
    inputIndex = 0;
    memset(inputBuffer, 0, sizeof(inputBuffer));

    while (inputIndex < maxDigits) {
        KeyEvent ev = KEYBOARD_READ_KEY();
        if (ev.type == KEY_NONE) continue;

        if (ev.type == KEY_CHAR) {
            if (inputIndex < MAX_INPUT_LEN) {
                inputBuffer[inputIndex++] = ev.value;
            }
        } else if (ev.type == KEY_CLEAR) {
            if (inputIndex > 0) {
                inputIndex--;
                inputBuffer[inputIndex] = '\0';
            }
        } else if (ev.type == KEY_ENTER) {
            break;
        }
    }
    return atoi(inputBuffer);
}

void KEYBOARD_GET_INPUT(char* buffer, int maxLen) {
    strncpy(buffer, inputBuffer, maxLen);
    buffer[maxLen - 1] = '\0';
}

void KEYBOARD_CLEAR_BUFFER() {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    inputIndex = 0;
}

char KEYBOARD_READ_CHAR() {
static const char* multiCharMap[10] = {
    "0",       // 0
    "1",       // 1
    "ABC2",    // 2
    "DEF3",    // 3
    "GHI4",    // 4
    "JKL5",    // 5
    "MNO6",    // 6
    "PQRS7",   // 7
    "TUV8",    // 8
    "WXYZ9"    // 9
};


    static int charIndex = 0;
    static char lastMultiKey = '\0';
    static unsigned long lastPressTime = 0;

    while (true) {
        KeyEvent ev = KEYBOARD_READ_KEY();
        if (ev.type == KEY_CHAR) {
            unsigned long now = millis();
            if (ev.value == lastMultiKey && (now - lastPressTime < 1000)) {
                charIndex = (charIndex + 1) % strlen(multiCharMap[ev.value - '0']);
            } else {
                lastMultiKey = ev.value;
                charIndex = 0;
            }
            lastPressTime = now;
            return multiCharMap[ev.value - '0'][charIndex];
        } else if (ev.type == KEY_ENTER || ev.type == KEY_CLEAR) {
            return ev.value;
        }
    }
}
