// keyboard.cpp
#include "keyboard.h"
#include "gpio.h"
#include <Arduino.h>
#include <string.h>
#include "oled.h"
#include "oled_queue.h"
#include <stdint.h>
#include "log.h"


static const uint8_t ROW_PINS[4] = { 201, 204, 203, 202 }; // R2, R4, R6, R7
static const uint8_t COL_PINS[3] = { 101, 100, 102 }; // C1, C3, C5
bool keyboardNeedsRefresh = false;

static const char KEYPAD[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

#define DEBOUNCE_DELAY 70
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







#define ANALOG_HIGH_THRESHOLD 900
#define DEBOUNCE_MS 70


#define INPUT_TIMEOUT_MS 5000

#define ERR_NO_INPUT_TIMEOUT    1000000001UL
#define ERR_INCOMPLETE_TIMEOUT  1000000002UL
#define ERR_KEYPAD_TIMEOUT 150u


static const uint8_t KEYPAD_NUM[4][3] = {
    { 1, 2, 3 },
    { 4, 5, 6 },
    { 7, 8, 9 },
    { 10, 0, 11 }   // 10=*  11=#
};


static uint32_t pow10u(uint8_t n)
{
    uint32_t v = 1u;
    while (n--)
        v *= 10u;
    return v;
}





#define NO_KEY            255u
#define COL_THRESHOLD    100u
#define KEY_TIMEOUT_MS   256u

#define POS_LONG_PRESS   0x80
#define POS_ROW_MASK     0x0C
#define POS_COL_MASK     0x03

#define POS_MAKE(row, col)   (((row) << 2) | (col))
#define POS_GET_ROW(pos)     (((pos) >> 2) & 0x03)
#define POS_GET_COL(pos)     ((pos) & 0x03)

uint8_t keypadScanPos(uint8_t scanSeconds = 5, uint8_t needLongPress = 0)
{
    unsigned long scanStart = millis();
    unsigned long scanTimeout = (unsigned long)scanSeconds * 1000UL;

    const unsigned long autoReturnMs = needLongPress ? 1000UL : 256UL;

    // ---- idle: rows LOW for fast column detect ----
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], LOW);

    delay(1);

    while ((millis() - scanStart) < scanTimeout)
    {
        int8_t colHit = -1;

        // -------- column scan --------
        for (uint8_t col = 0; col < 3; col++)
        {
            if (GPIO_READ(COL_PINS[col]) < COL_THRESHOLD)
            {
                colHit = col;
                break;
            }
            delay(3);
        }

        if (colHit < 0)
            continue;

        // -------- resolve row --------
        int8_t rowHit = -1;

        for (uint8_t row = 0; row < 4; row++)
        {
            for (uint8_t r = 0; r < 4; r++)
                GPIO_SET(ROW_PINS[r], HIGH);

            GPIO_SET(ROW_PINS[row], LOW);
            delay(3);

            if (GPIO_READ(COL_PINS[colHit]) < COL_THRESHOLD)
            {
                rowHit = row;
                break;
            }
        }

        // restore rows LOW
        for (uint8_t r = 0; r < 4; r++)
            GPIO_SET(ROW_PINS[r], LOW);

        if (rowHit < 0)
            continue;

        uint8_t pos = POS_MAKE(rowHit, colHit);
        unsigned long pressStart = millis();

        // -------- wait for release or auto-return --------
        while ((millis() - pressStart) < autoReturnMs)
        {
            if (GPIO_READ(COL_PINS[colHit]) >= COL_THRESHOLD)
            {
                // released early → NOT long press
                return pos;
            }
            delay(5);
        }

        // auto-return → LONG PRESS
        return pos | POS_LONG_PRESS;
    }

    // ---- global timeout ----
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], HIGH);

    return ERR_KEYPAD_TIMEOUT;
}



uint8_t keypadScan(uint8_t scanSeconds)
{
    unsigned long scanStart = millis();
    unsigned long scanTimeout = (unsigned long)scanSeconds * 1000UL;

    // ---- idle: rows LOW for fast column detect ----
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], LOW);

    delay(1);

    while ((millis() - scanStart) < scanTimeout)
    {
        int8_t colHit = -1;

        // -------- column scan --------
        for (uint8_t col = 0; col < 3; col++)
        {
            if (GPIO_READ(COL_PINS[col]) < COL_THRESHOLD)
            {
                colHit = col;
                break;
            }
            delay(3);
        }

        if (colHit < 0)
            continue;

        // -------- resolve row --------
        int8_t rowHit = -1;

        for (uint8_t row = 0; row < 4; row++)
        {
            for (uint8_t r = 0; r < 4; r++)
                GPIO_SET(ROW_PINS[r], HIGH);

            GPIO_SET(ROW_PINS[row], LOW);
            delay(3);

            if (GPIO_READ(COL_PINS[colHit]) < COL_THRESHOLD)
            {
                rowHit = row;
                break;
            }
        }

        // restore rows LOW
        for (uint8_t r = 0; r < 4; r++)
            GPIO_SET(ROW_PINS[r], LOW);

        if (rowHit < 0)
            continue;

        uint8_t key = KEYPAD_NUM[rowHit][colHit];
        unsigned long pressStart = millis();

        // -------- wait for release or timeout --------
        while ((millis() - pressStart) < KEY_TIMEOUT_MS)
        {
            if (GPIO_READ(COL_PINS[colHit]) >= COL_THRESHOLD)
                return key;   // released early

            delay(5);
        }

        // still pressed → auto return
        return key;
    }

    // ---- global timeout ----
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], HIGH);

    return ERR_KEYPAD_TIMEOUT;
}




uint32_t KEYBOARD_READ_NUMBER(uint8_t maxDigits,uint8_t time_scan)
{
    if (maxDigits == 0u) maxDigits = 1u;
    if (maxDigits > 9u)  maxDigits = 9u;

    uint32_t value = 0u;
    uint8_t digits = 0u;

    // largest valid + 1  → error sentinel
    const uint32_t ERROR_VALUE = pow10u(maxDigits);

    while (1)
    {
        uint8_t key = keypadScan(time_scan);

        // ---- TIMEOUT ----
        if (key == ERR_KEYPAD_TIMEOUT)
            return ERROR_VALUE;

        // ---- ENTER (# = 11) ----
        if (key == 11u)
        {
            return value;     // valid even if 0
        }

        // ---- DELETE (* = 10) ----
        if (key == 10u)
        {
            if (digits > 0u)
            {
                value /= 10u;
                digits--;
                OLED_DisplayNumber(value);
            }
            continue;
        }

        // ---- DIGIT 0–9 ----
        if (key <= 9u)
        {
            // already full → auto-return
            if (digits >= maxDigits)
                return value;

            value = value * 10u + (uint32_t)key;
            digits++;

            OLED_DisplayNumber(value);

            // full → return
            if (digits >= maxDigits)
                return value;

            // if value somehow exceeds max valid range → error
            if (value >= ERROR_VALUE)
                return ERROR_VALUE;
        }
    }
}


uint8_t TESTkeypadScan(void)
{
    unsigned long startTime = millis();

    // --- Force all rows LOW ---
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], LOW);

    delay(1); // settle

    while ((millis() - startTime) < 5000)
    {
        int8_t activeCol = -1;

        // -------- Column scan --------
        for (uint8_t col = 0; col < 3; col++)
        {
            uint16_t v = GPIO_READ(COL_PINS[col]);

            if (v < 100)
            {
                activeCol = col;
                Serial.print("COL ");
                Serial.print(col);
                Serial.println(" ACTIVE");
                break;  // stop immediately
            }

            delay(3); // column scan pacing
        }

        if (activeCol < 0)
            continue;

        // -------- Row scan for active column --------
        int8_t activeRow = -1;

        for (uint8_t row = 0; row < 4; row++)
        {
            // isolate row
            for (uint8_t r = 0; r < 4; r++)
                GPIO_SET(ROW_PINS[r], HIGH);

            GPIO_SET(ROW_PINS[row], LOW);
            delay(3);  // row scan pacing

            uint16_t v = GPIO_READ(COL_PINS[activeCol]);

            if (v < 100)
            {
                activeRow = row;
                Serial.print("ROW ");
                Serial.print(row);
                Serial.println(" ACTIVE");
                break;
            }
        }

        // restore all rows LOW
        for (uint8_t r = 0; r < 4; r++)
            GPIO_SET(ROW_PINS[r], LOW);

        Serial.println("----------------------");
        delay(100); // debounce / human pacing
    }

    // --- Exit: disable rows (no current draw) ---
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], HIGH);

    return 0;
}


char KEYBOARD_READ_CHAR(void)
{
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
    static uint8_t lastDigit = 255;
    static uint8_t charIndex = 0;
    static unsigned long lastPressTime = 0;

    while (1)
    {
        // wait forever, long-press enabled
        uint8_t pos = keypadScanPos(0, 1);
        if (pos == ERR_KEYPAD_TIMEOUT)
            continue;

        bool longPress = (pos & POS_LONG_PRESS) != 0;
        uint8_t row = POS_GET_ROW(pos);
        uint8_t col = POS_GET_COL(pos);
        uint8_t key = KEYPAD_NUM[row][col];

        // ----- special keys -----
        if (key == 11) return '#';   // ENTER
        if (key == 10) return '*';   // BACK / MODE

        // numeric only
        if (key > 9)
            continue;

        // ----- long press → numeric -----
        if (longPress)
        {
            return (char)('0' + key);
        }

        // ----- multi-tap (short press) -----
        unsigned long now = millis();

        if (key == lastDigit && (now - lastPressTime) < 400)
        {
            charIndex++;
            uint8_t len = strlen(multiCharMap[key]);
            if (charIndex >= len)
                charIndex = len - 1;
        }
        else
        {
            lastDigit = key;
            charIndex = 0;
        }

        lastPressTime = now;

        char result = multiCharMap[key][charIndex];

        // ----- shift toggle -----
        if (result == '>')
        {
            uppercase = !uppercase;
            OLED_Clear();
            OLED_DisplayText(uppercase ? "ABC MODE" : "abc mode", 2);
            delay(600);
            OLED_Clear();
            return '\0';
        }

        if (uppercase && isalpha(result))
            result = toupper(result);

        return result;
    }
}





void KEYBOARD_GET_INPUT(char* buffer, int maxLen) {
    strncpy(buffer, inputBuffer, maxLen);
    buffer[maxLen - 1] = '\0';
}

void KEYBOARD_CLEAR_BUFFER() {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    inputIndex = 0;
}

void KEYBOARD_READ_STRING(char* buffer, int maxLen, bool stopOnEnter) {
    memset(buffer, 0, maxLen);
    int index = 0;

    while (index < maxLen - 1) {
        delay(10);
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

char* KEYBOARD_READ(uint8_t mode, uint8_t special_key)
{
    static char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    uint8_t maxLen = (mode == 1) ? 20 : 7;
    uint8_t index = 0;
    bool isPassword = (mode == 2);
    uint8_t textSize = (mode == 1) ? 2 : 3;

    unsigned long lastActivity = millis();
    const unsigned long idleTimeout = 5000;   // 5s no activity → "@"

    for (;;)
    {
        // ---- idle timeout ----
        if (millis() - lastActivity > idleTimeout)
        {
            LOG_MIN("KEYBOARD_READ: idle timeout");
            return (char*)"@";
        }

        // wait for key (long-press enabled)
        uint8_t pos = keypadScanPos(1, 1);
        if (pos == ERR_KEYPAD_TIMEOUT)
            continue;

        lastActivity = millis();

        bool longPress = (pos & POS_LONG_PRESS);
        uint8_t row = POS_GET_ROW(pos);
        uint8_t col = POS_GET_COL(pos);
        uint8_t key = KEYPAD_NUM[row][col];

        // ---------- LONG PRESS # → SERVICE ----------
        if (key == 11 && longPress)
        {
            LOG_MIN("SERVICE MODE");
            return (char*)"@service";
        }

        // ---------- ENTER (#) ----------
        if (key == 11)
        {
            LOG_MIN("ENTER");
            return buffer;
        }

        // ---------- BACKSPACE (*) ----------
        if (key == 10)
        {
            if (index > 0)
                buffer[--index] = '\0';

            OLED_Clear();
            OLED_DisplayText(isPassword ? "**" : buffer, textSize);
            OLED_Update();
            continue;
        }

        // ---------- DIGITS ----------
        if (key <= 9)
        {
            if (index >= maxLen)
            {
                LOG_MIN("INPUT FULL");
                return buffer;
            }

            buffer[index++] = '0' + key;
            buffer[index] = '\0';

            OLED_Clear();
            OLED_DisplayText(isPassword ? "**" : buffer, textSize);
            OLED_Update();
        }
    }
}


char KEYBOARD_READ_CONTROL(uint8_t timeoutSeconds)
{
    unsigned long start = millis();
    unsigned long timeoutMs =
        (timeoutSeconds == 0) ? 0 : (unsigned long)timeoutSeconds * 1000UL;

    while (1)
    {
        uint8_t key = keypadScan(
            (timeoutSeconds == 0) ? 1 : timeoutSeconds
        );

        // no key detected in this scan window
        if (key == ERR_KEYPAD_TIMEOUT)
        {
            if (timeoutSeconds == 0)
                continue;  // forever mode

            if ((millis() - start) >= timeoutMs)
                return '\0';   // timeout reached

            continue;
        }

        // map keypad key → control action
        switch (key)
        {
            case 2:  return 'U';  // Up
            case 4:  return 'L';  // Left
            case 6:  return 'R';  // Right
            case 8:  return 'D';  // Down

            case 11: return 'E';  // # = Enter
            case 10: return 'B';  // * = Back

            case 1:  return '1';
            case 3:  return '3';
            case 7:  return '7';
            case 9:  return '9';

            default:
                // ignore 0, 5, etc.
                return '\0';
        }
    }
}




#define SCAN_DELAY_START_MS   64
#define SCAN_DELAY_STEP_MS     3
#define SCAN_DELAY_MAX_MS   500

bool KEYBOARD_ACTIVE(uint8_t seconds)
{
    unsigned long startTime = millis();
    unsigned long timeoutMs =
        (seconds == 0) ? 0 : (unsigned long)seconds * 1000UL;

    uint16_t scanDelay = SCAN_DELAY_START_MS;

    // rows LOW so any key press pulls column
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], LOW);

    delay(1);

    while (1)
    {
        // ----- scan columns -----
        for (uint8_t col = 0; col < 3; col++)
        {
            if (GPIO_READ(COL_PINS[col]) < COL_THRESHOLD)
            {
                // cleanup
                for (uint8_t r = 0; r < 4; r++)
                    GPIO_SET(ROW_PINS[r], HIGH);

                return true;
            }
            delay(3);
        }

        // ----- timeout check -----
        if (seconds != 0 && (millis() - startTime) >= timeoutMs)
        {
            for (uint8_t r = 0; r < 4; r++)
                GPIO_SET(ROW_PINS[r], HIGH);

            return false;
        }

        // ----- adaptive backoff -----
        delay(scanDelay);

        if (scanDelay < SCAN_DELAY_MAX_MS)
        {
            scanDelay += SCAN_DELAY_STEP_MS;
            if (scanDelay > SCAN_DELAY_MAX_MS)
                scanDelay = SCAN_DELAY_MAX_MS;
        }
    }
}



