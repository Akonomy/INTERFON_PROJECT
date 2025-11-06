#include "keyboard.h"

// ===========================
// Internal state
// ===========================

// Virtual pin mapping (based on your specification)
static const uint8_t colPins[KEYBOARD_COLS] = {200, 201, 202};  // COL1, COL3, COL5
static const uint8_t rowPins[KEYBOARD_ROWS] = {101, 102, 103, 104}; // ROW_2, ROW_4, ROW_6, ROW_7

// Key layout
static const char keyMap[KEYBOARD_ROWS][KEYBOARD_COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// Key state tracking
static bool keyState[KEYBOARD_ROWS][KEYBOARD_COLS] = {false};
static bool keyPressedFlag = false;
static char lastKeyPressed = 0;

// Configurable parameters
static uint16_t updateRateMs = KEYBOARD_UPDATE_RATE_MS;
static uint16_t highThreshold = KEYBOARD_HIGH_THRESHOLD;
static uint32_t lastUpdateTime = 0;

// ===========================
// Initialization
// ===========================
void KEYBOARD_Init() {
  GPIO_INIT();

  // Shift register pins as output (already handled in GPIO)
  // MUX pins are inputs (handled in GPIO)

  // Ensure shift register outputs start low
  for (int c = 0; c < KEYBOARD_COLS; c++) {
    GPIO_SET(colPins[c], LOW);
  }

  lastUpdateTime = millis();
  keyPressedFlag = false;
  lastKeyPressed = 0;
}

// ===========================
// Internal scan
// ===========================
static void scanKeypad() {
  for (int c = 0; c < KEYBOARD_COLS; c++) {
    // Activate one column
    GPIO_SET(colPins[c], HIGH);

    delayMicroseconds(50); // short settle time

    for (int r = 0; r < KEYBOARD_ROWS; r++) {
      int val = GPIO_READ(rowPins[r]);
      bool pressed = (val > highThreshold);

      if (pressed && !keyState[r][c]) {
        // Key just pressed
        keyState[r][c] = true;
        keyPressedFlag = true;
        lastKeyPressed = keyMap[r][c];
        Serial.printf("[KEYBOARD] Key Pressed: %c (Row %d, Col %d)\n", lastKeyPressed, r, c);
      } 
      else if (!pressed && keyState[r][c]) {
        // Key released
        keyState[r][c] = false;
      }
    }

    // Deactivate column before next
    GPIO_SET(colPins[c], LOW);
  }
}

// ===========================
// Public Update Function
// ===========================
void KEYBOARD_Update() {
  if (millis() - lastUpdateTime >= updateRateMs) {
    lastUpdateTime = millis();
    scanKeypad();
  }
}

// ===========================
// Check for new key
// ===========================
bool KEYBOARD_HasNewKey() {
  return keyPressedFlag;
}

// ===========================
// Get and clear last key
// ===========================
char KEYBOARD_GetLastKey() {
  if (!keyPressedFlag) return 0;
  keyPressedFlag = false;
  return lastKeyPressed;
}

// ===========================
// Peek last key (non-destructive)
// ===========================
char KEYBOARD_PeekLastKey() {
  return lastKeyPressed;
}

// ===========================
// Configuration setters
// ===========================
void KEYBOARD_SetUpdateRate(uint16_t rateMs) {
  updateRateMs = rateMs;
}

void KEYBOARD_SetThreshold(uint16_t threshold) {
  highThreshold = threshold;
}
