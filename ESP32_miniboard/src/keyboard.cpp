// keyboard.cpp
#include "keyboard.h"
#include "gpio.h"
#include <Arduino.h>
#include <string.h>
#include "oled.h"


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
    const unsigned long requiredHoldTime = 550;
    unsigned long pressStart = millis();

    while (true) {
        KeyEvent ev = KEYBOARD_READ_KEY();
        if (ev.value != key) {
            return false; // Released too early or wrong key
        }

        if (millis() - pressStart >= requiredHoldTime) {
            break;
        }
        delay(10);
    }

    // Wait for key release
    while (KEYBOARD_READ_KEY().value == key) {
        delay(5);
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
        " 0",       // 0 => Space, 0
        ".!?-1",      // 1 => . ! ?
        "ABC2",      // 2
        "DEF3",      // 3
        "GHI4",      // 4
        "JKL5",      // 5
        "MNO6",      // 6
        "PQRS7",     // 7
        "TUV8",      // 8
        "WXYZ9"      // 9
    };

    static char lastMultiKey = '\0';
    static int charIndex = 0;
    static unsigned long lastPressTime = 0;

    KeyEvent ev;
    unsigned long pressStart = 0;

    while (true) {
        ev = KEYBOARD_READ_KEY();

        if (ev.type == KEY_CHAR) {
            // Start tracking the press time
            pressStart = millis();

            // Wait for key release or timeout (i.e., hold detection)
            while (GPIO_DIGITAL_READ(ev.value) == HIGH) {
                delay(5);
                if (millis() - pressStart >= 600) {
                    // Long press detected → return digit
                    return ev.value;
                }
            }

            // If released before threshold, handle as multi-tap
            unsigned long now = millis();
            uint8_t keyIdx = ev.value - '0';
            if (keyIdx > 9) continue;

            if (ev.value == lastMultiKey && (now - lastPressTime < 400)) {
                // Cycle through letters
                charIndex++;
                if (charIndex >= strlen(multiCharMap[keyIdx])) {
                    charIndex = strlen(multiCharMap[keyIdx]) - 1;
                }
            } else {
                lastMultiKey = ev.value;
                charIndex = 0;
            }

            lastPressTime = now;

            // Wait to finalize input (short press)
            unsigned long confirmStart = millis();
            while (millis() - confirmStart < 400) {
                KeyEvent nextEv = KEYBOARD_READ_KEY();
                if (nextEv.type == KEY_CHAR) {
                    if (nextEv.value == ev.value) {
                        // cycle further
                        now = millis();
                        charIndex++;
                        if (charIndex >= strlen(multiCharMap[keyIdx])) {
                            charIndex = strlen(multiCharMap[keyIdx]) - 1;
                        }
                        lastPressTime = now;
                        confirmStart = millis(); // reset confirmation window
                    } else {
                        // Different key pressed: finalize current letter
                        lastMultiKey = nextEv.value;
                        return multiCharMap[keyIdx][charIndex];
                    }
                }
                delay(10);
            }

            // Timed out — return current letter
            return multiCharMap[keyIdx][charIndex];
        }
        else if (ev.type == KEY_ENTER || ev.type == KEY_CLEAR) {
            return ev.value;  // '#' or '*'
        }
    }
}







void KEYBOARD_READ_STRING(char* buffer, int maxLen, bool stopOnEnter) {
    memset(buffer, 0, maxLen);
    int index = 0;

    while (index < maxLen - 1) {
        char ch = KEYBOARD_READ_CHAR();

        if (ch == '#') { // Enter key
            if (stopOnEnter) break;
            continue;
        } else if (ch == '*') { // Clear key
            if (index > 0) {
                index--;
                buffer[index] = '\0';
            }
        } else {
            buffer[index++] = ch;
            buffer[index] = '\0';
        }
    }
}

char* KEYBOARD_READ(uint8_t mode)
{
    static char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    int maxLen = (mode == 1) ? 20 : 7;
    int index = 0;
    bool isPassword = (mode == 2);

    // Selectăm dimensiunea textului pentru afișaj
    uint8_t textSize = (mode == 1) ? 2 : 3;




    while (true)
    {
        char ch;

        if (mode == 1)
        {
            ch = KEYBOARD_READ_CHAR();
            if (ch == '\0') continue;
        }
        else
        {
            KeyEvent ev = KEYBOARD_READ_KEY();
            if (ev.type == KEY_NONE) continue;
            ch = ev.value;
        }


        // ENTER (#)
        if (ch == '#')
        {
            OLED_Clear();

            if (index == 0)
            {
                // return empty string
                buffer[0] = '`';

            }

            Serial.println(buffer);


            return buffer;
        }

        // BACKSPACE (*)
        if (ch == '*')
        {
            if (index > 0)
            {
                index--;
                buffer[index] = '\0';
            }

            OLED_Clear();

            if (!isPassword)
            {
                OLED_DisplayText(buffer, textSize);
            }
            else
            {
                OLED_DisplayText("``", textSize);

            }
            continue;
        }

        // FULL BUFFER
        if (index >= maxLen)
        {
            OLED_Clear();
            OLED_DisplayText("MAX LIMIT", 2);
            delay(400);
            OLED_Clear();

            if (!isPassword){
                OLED_DisplayText(buffer, textSize);
            }
            else{
                OLED_Clear();
                OLED_DisplayText("``", 3);
            }

            continue;
        }

        // ADD CHARACTER
        if (ch != '*' || ch != '#' || index < maxLen){
        buffer[index++] = ch;
        buffer[index] = '\0';

        OLED_Clear();
        if (!isPassword)
        {
            OLED_DisplayText(buffer, textSize);
        }
        else
        {
            OLED_Clear();
            OLED_DisplayText("``", 3);
            OLED_Update();
        }
    }

      OLED_Update();

    }
}
