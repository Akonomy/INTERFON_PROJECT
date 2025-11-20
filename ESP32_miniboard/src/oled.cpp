#include "oled.h"
#include <Wire.h>

// ===========================
// Internal state
// ===========================
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static uint32_t lastCharShowTime = 0;
static bool waitingToMask = false;
static String currentPassword = "";
static String maskedPassword = "";

// ===========================
// Initialization
// ===========================
void OLED_Init() {
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error: OLED display not found!"));
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(DEFAULT_TEXT_SIZE);
  display.setCursor(DEFAULT_TEXT_X, DEFAULT_TEXT_Y);
  display.display();
}

// ===========================
// Clear screen
// ===========================
void OLED_Clear() {
  display.clearDisplay();
  display.display();
}

// ===========================
// Display number
// ===========================
void OLED_DisplayNumber(uint8_t number) {
  display.clearDisplay();
  display.setTextSize(DEFAULT_TEXT_SIZE);
  display.setCursor(DEFAULT_TEXT_X, DEFAULT_TEXT_Y);
  display.print(number);
  display.display();
}

// ===========================
// Display text
// ===========================
void OLED_DisplayText(const String &text, uint8_t size) {
  display.clearDisplay();
  display.setTextSize(size);   // folosește dimensiunea cerută
  display.setCursor(DEFAULT_TEXT_X, DEFAULT_TEXT_Y);
  display.print(text);
  display.display();
}


// ===========================
// Display debug
// ===========================
void OLED_DisplayDebug(const String &text, int x, int y, int size, bool clear) {
  if (clear) display.clearDisplay();
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(text);
  display.display();
}

// ===========================
// Password Display Logic
// ===========================
//
// Shows characters as they’re entered:
// - Last char is shown for a short time (PASSWORD_SHOW_TIME)
// - Then replaced by '*'
// - Reset option clears stored password
//
void OLED_DisplayPassword(const String &input, bool reset) {
  if (reset) {
    currentPassword = "";
    maskedPassword = "";
    OLED_Clear();
    return;
  }

  // New character(s) entered
  if (input.length() > currentPassword.length()) {
    char newChar = input[input.length() - 1];
    currentPassword = input;
    maskedPassword = String(maskedPassword) + newChar;
    waitingToMask = true;
    lastCharShowTime = millis();
  }

  display.clearDisplay();
  display.setTextSize(DEFAULT_TEXT_SIZE);
  display.setCursor(DEFAULT_TEXT_X, DEFAULT_TEXT_Y);
  display.print(maskedPassword);
  display.display();
}

// ===========================
// Update timer-based masking
// ===========================
//
// Must be called repeatedly in loop() to mask
// the last shown password character after timeout.
//
void OLED_Update() {
  if (waitingToMask && millis() - lastCharShowTime >= PASSWORD_SHOW_TIME) {
    // Replace last shown char with '*'
    if (maskedPassword.length() > 0) {
      maskedPassword.setCharAt(maskedPassword.length() - 1, '*');
    }
    waitingToMask = false;

    display.clearDisplay();
    display.setTextSize(DEFAULT_TEXT_SIZE);
    display.setCursor(DEFAULT_TEXT_X, DEFAULT_TEXT_Y);
    display.print(maskedPassword);
    display.display();
  }
}
