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





#define NO_KEY            255u // eroare fara tasta
#define COL_THRESHOLD    100u //nu modifica aici e nivelul care se considera tasta activa
#define KEY_TIMEOUT_MS   400u  //timp dupa care se returneaza automat chiar daca tasta e inca apasata

#define POS_LONG_PRESS   0x80
#define POS_ROW_MASK     0x0C
#define POS_COL_MASK     0x03

#define POS_MAKE(row, col)   (((row) << 2) | (col))
#define POS_GET_ROW(pos)     (((pos) >> 2) & 0x03)
#define POS_GET_COL(pos)     ((pos) & 0x03)


#define POS_FLAG_SHORT   0x20  // bit 5
#define POS_FLAG_NORMAL  0x40  // bit 6
#define POS_FLAG_LONG    0x80  // bit 7



// Extract raw row/col position (lower bits)
#define POS_GET_POS(p)    ((p) & 0x0F)

// Extract keypad key number (using KEYPAD_NUM table)
#define POS_GET_KEY(p) \
    (KEYPAD_NUM[POS_GET_ROW(POS_GET_POS(p))][POS_GET_COL(POS_GET_POS(p))])

// Extract press type flags
#define POS_GET_FLAG(p)  ((p) & (POS_FLAG_SHORT | POS_FLAG_NORMAL | POS_FLAG_LONG))




#define POS_IS_SHORT(p)   ((p) & POS_FLAG_SHORT)
#define POS_IS_NORMAL(p)  ((p) & POS_FLAG_NORMAL)
#define POS_IS_LONG(p)    ((p) & POS_FLAG_LONG)

uint8_t keypadScanPos(
    uint8_t scanSeconds = 5,
    uint8_t needLongPress = 0,
    uint8_t needFastReturn = 0
)
{
    unsigned long scanStart = millis();
    unsigned long scanTimeout = (unsigned long)scanSeconds * 1000UL;

    const unsigned long autoReturnMs = needLongPress ? 1000UL : 450UL;
    unsigned long noKeyStart = 0;

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

        // -------- no activity --------
        if (colHit < 0)
        {
            if (needFastReturn)
            {
                if (noKeyStart == 0)
                    noKeyStart = millis();
                else if (millis() - noKeyStart >= 350)
                    return ERR_KEYPAD_TIMEOUT; // fast exit
            }
            continue;
        }

        // activity detected → reset idle timer
        noKeyStart = 0;

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
                unsigned long dt = millis() - pressStart;

                if (!needLongPress)
                {
                    if (dt < 155)  //timp de apasare tasta pt a fi considerata short press
                        return pos | POS_FLAG_SHORT;
                    else
                        return pos | POS_FLAG_NORMAL;
                }

                return pos | POS_FLAG_NORMAL;
            }
            delay(5);
        }

        return pos | POS_FLAG_LONG;
    }

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


void TESTkeypadScan(void)

{
    unsigned long testStart = millis();

    Serial.println("=== KEY PRESS DURATION TEST (10s) ===");

    // rows LOW for fast detection
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], LOW);

    delay(1);

    while ((millis() - testStart) < 100000UL)
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
            delay(1);
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
            delay(1);

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

        // -------- wait for release --------
        while (GPIO_READ(COL_PINS[colHit]) < COL_THRESHOLD)
        {
            delay(1);
        }

        unsigned long duration = millis() - pressStart;

        Serial.print("POS row=");
        Serial.print(POS_GET_ROW(pos));
        Serial.print(" col=");
        Serial.print(POS_GET_COL(pos));
        Serial.print("  duration=");
        Serial.print(duration);
        Serial.println(" ms");

        // small debounce so one press prints once
        delay(200);
    }

    // cleanup
    for (uint8_t r = 0; r < 4; r++)
        GPIO_SET(ROW_PINS[r], HIGH);

    Serial.println("=== TEST END ===");
}


// ================================
//  MULTITAP KEYBOARD MODULE
// ================================

// -------- config --------
#define MAX_CHARS       42
#define MAX_STRING_LEN  43     // 42 chars + null
#define KB_IDLE_TIMEOUT 15000  // 15 seconds in ms

// -------- table mode --------
typedef enum {
    TABLE_LOWER = 0,
    TABLE_UPPER = 1
} KeyTable;

// -------- lookup table --------
static const char LUT[2][10][6] = {
    {   // LOWER
        " 0>", ".!?-1", "abc2", "def3", "ghi4",
        "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"
    },
    {   // UPPER
        " 0<", ".!?-1", "ABC2", "DEF3", "GHI4",
        "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9"
    }
};

// -------- packing & unpacking --------
#define MAKE_KEYSTATE(key, state)   (((key & 0x0F) << 3) | (state & 0x07))
#define KEY_FROM_KEYSTATE(v)        (((v) >> 3) & 0x0F)
#define STATE_FROM_KEYSTATE(v)      ((v) & 0x07)

// -------- globals --------
volatile KeyTable g_activeTable = TABLE_LOWER;
static uint8_t ksBuffer[MAX_CHARS];
static uint8_t ksLen = 0;

// ================================
// helper: get max multitab states
// ================================
static uint8_t keyMaxState(uint8_t key)
{
    if (key == 10) return 0;    // *
    if (key == 11) return 7;    // #

    if (key == 0) return 2;     // 3 states
    if (key == 1 || key == 7 || key == 9)
        return 4;               // 5 states

    return 3;                   // default 4 states
}

// ================================
// helper: LUT + mode switch (< >)
// ================================
char LUT_GetCharWithModeSwitch(uint8_t key, uint8_t state)
{
    char ch = LUT[g_activeTable][key][state];

    if (key == 0)
    {
        if (ch == '>')
            g_activeTable = TABLE_UPPER;
        else if (ch == '<')
            g_activeTable = TABLE_LOWER;
    }

    return ch;
}

// ================================
// MULTITAP on a single keypress
// ================================
uint8_t keypadMultiTap(uint8_t pos)
{

    uint8_t state = 0;

    uint8_t res = keypadScanPos(5, 0, 0);
    if (res == ERR_KEYPAD_TIMEOUT)
        return ERR_KEYPAD_TIMEOUT;

    uint8_t key       = POS_GET_KEY(res);
    uint8_t pressType = POS_GET_FLAG(res);
    uint8_t maxState  = keyMaxState(key);

    // fixed keys
    if (key == 10) return MAKE_KEYSTATE(10, 0); // delete
    if (key == 11) return MAKE_KEYSTATE(11, 7); // enter

    // long press → last state
    if (pressType == POS_FLAG_LONG){
        char chs = LUT_GetCharWithModeSwitch(key, maxState);
        OLED_DrawCharAtPos(2, pos, chs);
        delay(200);
        return MAKE_KEYSTATE(key, maxState);
    }

    // normal press → state 0
    if (pressType == POS_FLAG_NORMAL)
    {
        char chl = LUT_GetCharWithModeSwitch(key, 0);
        OLED_DrawCharAtPos(2, pos, chl);
        delay(200);
        return MAKE_KEYSTATE(key, 0);
    }

    // short press → cycle
    if (pressType == POS_FLAG_SHORT)
    {
        state = 0;

        while (1)
        {
            char ch = LUT_GetCharWithModeSwitch(key, state);
            OLED_DrawCharAtPos(2, pos, ch);
            uint8_t r = keypadScanPos(2, 0, 1);

            if (r == ERR_KEYPAD_TIMEOUT)
                return MAKE_KEYSTATE(key, state);

            uint8_t k2 = POS_GET_KEY(r);
            uint8_t p2 = POS_GET_FLAG(r);

            if (k2 != key)
                return MAKE_KEYSTATE(key, state);

            if (p2 == POS_FLAG_SHORT)
            {
                if (state < maxState) state++;
                else return MAKE_KEYSTATE(key, state);
            }
            else
                return MAKE_KEYSTATE(key, state);
        }
    }

    return ERR_KEYPAD_TIMEOUT;
}

// ================================
// BUILD PACKED BUFFER (15s timeout)
// ================================
uint8_t keypadBuildPacked(void)
{
    ksLen = 0;
    g_activeTable = TABLE_LOWER;

    uint8_t pos = 0;
    unsigned long lastActivity = millis();

    while (ksLen < MAX_CHARS)
    {
        // global idle timeout
        if (millis() - lastActivity >= KB_IDLE_TIMEOUT)
            return 0; // cancel/timeout

        uint8_t ks = keypadMultiTap(pos);

        if (ks == ERR_KEYPAD_TIMEOUT)
            continue;

        lastActivity = millis();

        uint8_t key   = KEY_FROM_KEYSTATE(ks);
        uint8_t state = STATE_FROM_KEYSTATE(ks);

        //table change

        if (key==0 && state==2){


                delay(210);
                OLED_ClearCharAtPos(2, pos);
                ksBuffer[ksLen++] = ks;
                continue;


        }

        // delete
        if (key == 10)
        {
            if (ksLen > 0)
            {
                ksLen--;
                pos--;
                OLED_ClearCharAtPos(2, pos);
            }
            continue;
        }

        // enter/finish
        if (key == 11){
            return ksLen;
        }

        // append
        ksBuffer[ksLen++] = ks;

        char ch = LUT_GetCharWithModeSwitch(key, state);
        OLED_DrawCharAtPos(2, pos, ch);
        pos++;
    }

    return ksLen;
}

// ================================
// DECODE PACKED BUFFER TO TEXT
// ================================
void decodePackedBuffer(char *out)
{
    g_activeTable = TABLE_LOWER;

    for (uint8_t i = 0; i < ksLen; i++)
    {
        uint8_t key   = KEY_FROM_KEYSTATE(ksBuffer[i]);
        uint8_t state = STATE_FROM_KEYSTATE(ksBuffer[i]);

        char ch = LUT_GetCharWithModeSwitch(key, state);

        // swallow mode-switch characters
        if (key == 0 && (ch == '>' || ch == '<'))
            continue;

        *out++ = ch;
    }

    *out = '\0';
}


// ================================
// HIGH LEVEL API
// ================================
char* KEYBOARD_READ_STRING(void)
{
    static char decoded[MAX_STRING_LEN];

    uint8_t n = keypadBuildPacked();

    // cancel / timeout / empty input → nullptr
    if (n == 0)
        return nullptr;

    decodePackedBuffer(decoded);
    return decoded;
}





void keypadMultiTapTest(void)
{
    uint8_t count = 0;

    printf("MultiTap test start (max 10 keys)\n");

    while (count < 20)
    {
        uint8_t v = keypadMultiTap(count);  // displayPosition = count

        if (v == ERR_KEYPAD_TIMEOUT)
        {
            printf("[%u] TIMEOUT\n", count);
            break;
        }

        uint8_t key   = KEY_FROM_KEYSTATE(v);
        uint8_t state = STATE_FROM_KEYSTATE(v);

        printf("[%u] RAW=0x%02X  KEY=%u  STATE=%u\n",
               count, v, key, state);

        count++;
    }

    printf("MultiTap test end (%u entries)\n", count);
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
            LOG_MIN("idle timeout");
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



