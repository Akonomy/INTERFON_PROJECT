// keyboard.cpp
#include "keyboard.h"
#include "gpio.h"
#include <Arduino.h>
#include <string.h>
#include "oled.h"


static const uint8_t ROW_PINS[4] = { 201, 204, 203, 202 }; // R2, R4, R6, R7
static const uint8_t COL_PINS[3] = { 101, 100, 102 }; // C1, C3, C5
bool keyboardNeedsRefresh = false;

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
        " 0>",       // 0 => space, 0, shift toggle
        ".!?-1",
        "abc2",
        "def3",
        "ghi4",
        "jkl5",
        "mno6",
        "pqrs7",
        "tuv8",
        "wxyz9"
    };

    static bool uppercase = false;
    static char lastMultiKey = '\0';
    static int charIndex = 0;
    static unsigned long lastPressTime = 0;

    KeyEvent ev;
    unsigned long pressStart = 0;

    while (true) {
        ev = KEYBOARD_READ_KEY();

        if (ev.type == KEY_CHAR) {
            char key = ev.value;

            pressStart = millis();
            while (GPIO_DIGITAL_READ(ev.value) == HIGH) {
                delay(5);
                if (millis() - pressStart >= 600) {
                    return key; // long press → number
                }
            }

            unsigned long now = millis();
            uint8_t keyIdx = key - '0';
            if (keyIdx > 9) continue;

            if (key == lastMultiKey && (now - lastPressTime < 400)) {
                charIndex++;
                if (charIndex >= strlen(multiCharMap[keyIdx])) {
                    charIndex = strlen(multiCharMap[keyIdx]) - 1;
                }
            } else {
                lastMultiKey = key;
                charIndex = 0;
            }

            lastPressTime = now;

            // Wait for next key or timeout
            unsigned long confirmStart = millis();
            while (millis() - confirmStart < 400) {
                KeyEvent nextEv = KEYBOARD_READ_KEY();
                if (nextEv.type == KEY_CHAR) {
                    if (nextEv.value == key) {
                        charIndex++;
                        if (charIndex >= strlen(multiCharMap[keyIdx])) {
                            charIndex = strlen(multiCharMap[keyIdx]) - 1;
                        }
                        lastPressTime = millis();
                        confirmStart = millis(); // restart
                    } else {
                        lastMultiKey = nextEv.value;
                        break;
                    }
                }
                delay(10);
            }

            // FINAL SELECTION
            char result = multiCharMap[keyIdx][charIndex];

            if (result == '>') {
                // toggle case mode
                uppercase = !uppercase;
                OLED_Clear();
                OLED_DisplayText(uppercase ? "ABC MODE" : "abc mode", 2);
                delay(600);
                OLED_Clear();

                // signal that we want to refresh buffer text after this
                keyboardNeedsRefresh = true;

                return '\0'; // don't insert anything
            }


            if (uppercase && isalpha(result)) {
                result = toupper(result);
            }

            return result;
        }

        else if (ev.type == KEY_ENTER || ev.type == KEY_CLEAR) {
            return ev.value;
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

    uint8_t textSize = (mode == 1) ? 2 : 3;

    const unsigned long inputTimeout = 15000; // 15 seconds to finish typing
    const unsigned long idleTimeout = 5000;   // 5 seconds no key at all = return

    unsigned long lastKeyTime = millis();     // last key press time
    unsigned long startTime = millis();       // total idle time

    while (true)
    {

         if (keyboardNeedsRefresh) {
                OLED_Clear();
                if (!isPassword)
                    OLED_DisplayText(buffer, textSize);
                else
                    OLED_DisplayText("``", 3);

                keyboardNeedsRefresh = false; // reset flag
            }
        char ch;
        KeyEvent ev;

        if (mode == 1)
        {
            ch = KEYBOARD_READ_CHAR();
            if (ch == '\0') {
                // check timeouts
                if (millis() - lastKeyTime > inputTimeout && index > 0) {
                    Serial.println("⏱️ INPUT TIMEOUT → discarding partial input");
                    return (char*)"@TIME";
                }

                if (millis() - startTime > idleTimeout && index == 0) {
                    Serial.println("💤 IDLE TIMEOUT → no key pressed at all");
                    return (char*)"@NONE";
                }

                delay(50);
                continue;
            }


        }
        else
        {
            ev = KEYBOARD_READ_KEY();
            if (ev.type == KEY_NONE) {
                // same timeout checks
                if (millis() - lastKeyTime > inputTimeout && index > 0) {
                    Serial.println("⏱️ INPUT TIMEOUT → discarding partial input");
                    return (char*)"@TIME";
                }

                if (millis() - startTime > idleTimeout && index == 0) {
                   // Serial.println("💤 IDLE TIMEOUT → no key pressed at all");
                    return (char*)"@NONE";
                }

                delay(50);
                continue;
            }
            ch = ev.value;
        }

        // key was pressed — reset timers
        lastKeyTime = millis();
        startTime = millis(); // restart idle timeout too



        // ENTER (#)
        if (ch == '#')
        {
            OLED_Clear();

            if (index == 0)
            {
                buffer[0] = '`';
            }

            Serial.println("🔓 ENTER pressed → returning input:");
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
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("``", textSize);
            continue;
        }

        // FULL BUFFER
        if (index >= maxLen)
        {
            OLED_Clear();
            OLED_DisplayText("MAX LIMIT", 2);
            delay(400);
            OLED_Clear();
            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("``", 3);
            continue;
        }

        // ADD CHARACTER
        if (ch != '*' && ch != '#') {
            buffer[index++] = ch;
            buffer[index] = '\0';

            OLED_Clear();
            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else {
                OLED_Clear();
                OLED_DisplayText("``", 3);
            }
        }

        OLED_Update();
    }
}




char KEYBOARD_READ_CONTROL() {
    while (true) {
        KeyEvent ev = KEYBOARD_READ_KEY();
        if (ev.type == KEY_NONE) continue;

        char key = ev.value;

        switch (key) {
            case '2': return 'U';  // Up
            case '4': return 'L';  // Left
            case '6': return 'R';  // Right
            case '8': return 'D';  // Down
            case '#': return 'E';  // Enter
            case '*': return 'B';  // Back
            case '1': return '1';
            case '3': return '3';
            case '7': return '7';
            case '9': return '9';
            default:
                // Ignore other keys
                return '\0';
        }
    }
}
