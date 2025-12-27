// keyboard.cpp
#include "keyboard.h"
#include "gpio.h"
#include <Arduino.h>
#include <string.h>
#include "oled.h"
#include "oled_queue.h"

static const uint8_t ROW_PINS[4] = { 201, 204, 203, 202 }; // R2, R4, R6, R7
static const uint8_t COL_PINS[3] = { 101, 100, 102 }; // C1, C3, C5
bool keyboardNeedsRefresh = false;

static const char KEYPAD[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

#define DEBOUNCE_DELAY 19
#define HOLD_THRESHOLD 300
#define MAX_INPUT_LEN 16

static char inputBuffer[MAX_INPUT_LEN + 5];
static int inputIndex = 0;
static char lastKey = '\0';

void KEYBOARD_INIT() {
    for (int i = 0; i < 4; ++i) {
        GPIO_SET(ROW_PINS[i], LOW);
    }
    memset(inputBuffer, 0, sizeof(inputBuffer));
    inputIndex = 0;
}



#include <stdint.h>

uint8_t KEYBOARD_ANY_PRESSED() {
    // 1) drive all rows HIGH
    for (int i = 0; i < 4; i++) {
        GPIO_SET(ROW_PINS[i], HIGH);
    }

    delayMicroseconds(50);  // let mux settle

    // 2) scan all columns
    for (int col = 0; col < 3; col++) {
        if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH) {

            // restore rows LOW before exit
            for (int i = 0; i < 4; i++) {
                GPIO_SET(ROW_PINS[i], LOW);
            }

            return 1;   // key present
        }
    }

    // 3) restore rows LOW when nothing detected
    for (int i = 0; i < 4; i++) {
        GPIO_SET(ROW_PINS[i], LOW);
    }

    return 0;   // no key
}



uint8_t KEYBOARD_ACTIVE_PULSE()
{
    const unsigned long windowMs = 1000;   // ~1 second total window
    const unsigned long stepMs   = 90;     // check every ~90 ms

    unsigned long start = millis();
    unsigned long lastCheck = 0;

    // drive all rows HIGH once
    for (int r = 0; r < 4; r++) {
        GPIO_SET(ROW_PINS[r], HIGH);
    }

    while (millis() - start < windowMs)
    {
        unsigned long now = millis();

        // wait until 90ms boundary
        if (now - lastCheck < stepMs)
            continue;

        lastCheck = now;

        // ---- sequential column scan ----
        for (int col = 0; col < 3; col++)
        {
            if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH)
            {
                // restore rows LOW before exit
                for (int r = 0; r < 4; r++) {
                    GPIO_SET(ROW_PINS[r], LOW);
                }

                return 1;   // activity detected
            }
        }
    }

    // ---- timeout, no keyboard activity ----
    for (int r = 0; r < 4; r++) {
        GPIO_SET(ROW_PINS[r], LOW);
    }

    return 0;
}



uint8_t KEYBOARD_ACTIVE()
{
    static unsigned long last = 0;
    static unsigned long interval = 50;        // start fast
    const unsigned long maxInterval = 1200;    // cap

    unsigned long now = millis();

    // Too soon → do nothing
    if (now - last < interval) {
        return 0;
    }

    last = now;

    // Cheap check — no full scan
    if (KEYBOARD_ANY_PRESSED()) {
        interval = 50;    // go fast again
        return 1;         // wake event
    }

    // idle → back off a bit
    interval += 80;
    if (interval > maxInterval) {
        interval = maxInterval;
    }

    // jitter to avoid robotic timing
    interval += (now & 0x3F);

    return 0;
}


static bool isKeyStillPressed(char key)
{
    // rescan matrix ONLY for that key position
    for (int row = 0; row < 4; ++row) {
        GPIO_SET(ROW_PINS[row], HIGH);
        delayMicroseconds(50);

        for (int col = 0; col < 3; ++col) {
            if (KEYPAD[row][col] == key) {
                bool pressed = GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH;
                GPIO_SET(ROW_PINS[row], LOW);
                return pressed;
            }
        }
        GPIO_SET(ROW_PINS[row], LOW);
    }
    return false;
}

static bool waitForKeyRelease(char key)
{
    unsigned long start = millis();

    while (isKeyStillPressed(key)) {

        if (millis() - start > 300) {
            // timeout auto-release
            return true;
        }

        delay(5);   // yields to watchdog
    }

    return true;
}








KeyEvent KEYBOARD_READ_KEY(uint8_t mode)

{
 for (int row = 0; row < 4; ++row) {

        GPIO_SET(ROW_PINS[row], HIGH);

        for (int col = 0; col < 3; ++col) {

            if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH) {

                delay(DEBOUNCE_DELAY);

                if (GPIO_DIGITAL_READ(COL_PINS[col]) == HIGH) {

                    char key = KEYPAD[row][col];

                    unsigned long start = millis();

                    // stay here while pressed to measure hold duration
                    while (isKeyStillPressed(key)) {
                        unsigned long held = millis() - start;

                        // -------- MODE 1 : long press * and # only --------
                        if (mode == 1) {

                            if (key == '*' && held >= 1256) {
                                waitForKeyRelease(key);

                                GPIO_SET(ROW_PINS[row], LOW);

                                KeyEvent ev = { KEY_CHAR, '[' };
                                lastKey = '[';
                                return ev;
                            }

                            if (key == '#' && held >= 1256) {
                                waitForKeyRelease(key);

                                GPIO_SET(ROW_PINS[row], LOW);

                                KeyEvent ev = { KEY_CHAR, ']' };
                                lastKey = ']';
                                return ev;
                            }
                        }

                        // -------- MODE 2 : long press ANY key (except * #) --------
                        if (mode == 2) {

                            if (key != '*' && key != '#' && held >= 1000) {

                                waitForKeyRelease(key);

                                GPIO_SET(ROW_PINS[row], LOW);

                                // return "~<key>" as single char '~' + key
                                // e.g., long press '5' → '~5'
                                KeyEvent ev;
                                ev.type = KEY_CHAR;
                                ev.value = '~';   // base marker

                                // You can alternatively pack both if desired,
                                // but requirement says: return ~ + key pressed
                                // If you want literal two characters, adjust buffer logic.

                                lastKey = '~';
                                return ev;
                            }
                        }
                    }

                    // ---------- Normal flow (no long press detected) ----------

                    waitForKeyRelease(key);

                    GPIO_SET(ROW_PINS[row], LOW);

                    KeyEvent ev;
                    ev.value = key;

                    if (key == '#') {
                        ev.type = KEY_ENTER;
                    }
                    else if (key == '*') {
                        ev.type = KEY_CLEAR;
                    }
                    else {
                        ev.type = KEY_CHAR;
                    }

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
    LOG_MIN("KEYBOARD_READ: ENTER");

    static char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    int maxLen = (mode == 1) ? 20 : 7;
    int index = 0;
    bool isPassword = (mode == 2);

    uint8_t textSize = (mode == 1) ? 2 : 3;

    const unsigned long inputTimeout = 15000; // 15 seconds to finish typing
    const unsigned long idleTimeout = 5000;   // 5 seconds no key at all = return

    unsigned long lastKeyTime = millis();
    unsigned long startTime = millis();

    LOG("KEYBOARD_READ: initial timers set");

    while (true)
    {
        if (keyboardNeedsRefresh) {
            LOG("KEYBOARD_REFRESH");
            OLED_Clear();

            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("**", 3);

            keyboardNeedsRefresh = false;
        }

        char ch;
        KeyEvent ev;

        // ---------- MODE 1 ----------
        if (mode == 1)
        {
            ch = KEYBOARD_READ_CHAR();

            if (ch == '\0') {

                // ---- TIMEOUTS ----
                if (millis() - lastKeyTime > inputTimeout && index > 0) {
                    LOG_MIN("TIMEOUT: INPUT PARTIAL DISCARDED");
                    delay(200);
                    return (char*)"@TIME";
                }

                if (millis() - startTime > idleTimeout && index == 0) {
                    LOG_MIN("TIMEOUT: NO KEY ACTIVITY");
                    delay(200);
                    return (char*)"@NONE";
                }

                delay(50);
                continue;
            }
        }
        else
        // ---------- EVENT MODE ----------
        {
           // ---------- EVENT MODE ----------
{
            // daca bufferul e gol -> foloseste modul 1 (long press *, #)
            if (index == 0){
                ev = KEYBOARD_READ_KEY(1);
            }
            else{
                ev = KEYBOARD_READ_KEY();
            }

            if (ev.type == KEY_NONE) {

                if (millis() - lastKeyTime > inputTimeout && index > 0) {
                    LOG_MIN("TIMEOUT: INPUT PARTIAL DISCARDED");
                    return (char*)"@TIME";
                }

                if (millis() - startTime > idleTimeout && index == 0) {
                    LOG_MIN("TIMEOUT: NO KEY ACTIVITY");
                    return (char*)"@NONE";
                }

                delay(50);
                continue;
            }

            ch = ev.value;
        }

        // key pressed — reset timeouts
        lastKeyTime = millis();
        startTime   = millis();



        //------------SERVICE------------

          if (ch == ']')
        {
            LOG_MIN("SERVICE MODE TRIGGER");

            OLED_Clear();
            OLED_DisplayText("***", 2);
            delay(200);

            return (char*)"@service";
        }




        // ---------- ENTER (#) ----------
        if (ch == '#')
        {
            LOG_MIN("ENTER PRESSED — RETURNING INPUT");

            OLED_Clear();

            if (index == 0)
                buffer[0] = 0;

            LOG(buffer);   // full log prints content
            return buffer;
        }

        // ---------- BACKSPACE (*) ----------
        if (ch == '*')
        {
            LOG("BACKSPACE");

            if (index > 0) {
                index--;
                buffer[index] = '\0';
            }

            OLED_Clear();
            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("**", textSize);

            continue;
        }

        // ---------- FULL BUFFER ----------
        if (index >= maxLen)
        {
            LOG_MIN("INPUT LIMIT REACHED");

            OLED_Clear();
            OLED_DisplayText("MAX LIMIT", 2);
            delay(400);

            OLED_Clear();
            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("**", 3);

            continue;
        }

        // ---------- NORMAL CHAR ----------
        if (ch != '*' && ch != '#') {

            LOG("CHAR ADDED");

            buffer[index++] = ch;
            buffer[index] = '\0';

            OLED_Clear();

            if (!isPassword)
                OLED_DisplayText(buffer, textSize);
            else
                OLED_DisplayText("**", 3);
        }

        OLED_Update();
    }
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
