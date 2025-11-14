#include "gpio.h"

// ===========================
// Hardware Config
// ===========================
#define DATA_PIN 7
#define CLOCK_PIN 6
#define LATCH_PIN 5

#define S0 0
#define S1 1
#define S2 3
#define MUX_INPUT 4

// Reserved pins (for I2C or USART)
#define RESERVED_SDA 8
#define RESERVED_SCL 9
#define RESERVED_UART_TX 20
#define RESERVED_UART_RX 21

// ===========================
// Internal State
// ===========================
static uint8_t shiftRegState = 0x00;

// ===========================
// Helper Functions
// ===========================
static void updateShiftRegister() {
  digitalWrite(LATCH_PIN, LOW);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CLOCK_PIN, LOW);
    digitalWrite(DATA_PIN, (shiftRegState >> i) & 0x01);
    digitalWrite(CLOCK_PIN, HIGH);
  }
  digitalWrite(LATCH_PIN, HIGH);
}

static void selectMuxChannel(uint8_t ch) {
  digitalWrite(S0, ch & 0x01);
  digitalWrite(S1, (ch >> 1) & 0x01);
  digitalWrite(S2, (ch >> 2) & 0x01);
  delayMicroseconds(50);
}

// ===========================
// Initialization
// ===========================
void GPIO_INIT() {
  // Shift register pins
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  // MUX control pins
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(MUX_INPUT, INPUT);

  // Initialize shift register
  shiftRegState = 0x00;
  updateShiftRegister();
}

// ===========================
// Validation
// ===========================
bool GPIO_IS_VALID(uint8_t pin) {
  // Valid ESP physical pins (0–10)
  if (pin <= 10) {
    // block reserved pins
    if (pin == 0 || pin == 1 || pin == 3 || pin == 4 ||
        pin == 5 || pin == 6 || pin == 7 ||
        pin == 8 || pin == 9)
      return false; // reserved
    return true; // e.g., 2 and 10 are allowed
  }

  // Virtual MUX pins
  if (pin >= 100 && pin <= 107) return true;

  // Virtual Shift Register pins
  if (pin >= 200 && pin <= 207) return true;

  // Otherwise invalid
  return false;
}

// ===========================
// I/O Functions
// ===========================
void GPIO_SET(uint8_t pin, bool state) {
  if (!GPIO_IS_VALID(pin)) {
    Serial.printf("GPIO_SET ERROR: invalid or reserved pin %d\n", pin);
    return;
  }

  if (pin <= 10) {
    // Physical ESP pin
    digitalWrite(pin, state ? HIGH : LOW);
  }
  else if (pin >= 200 && pin <= 207) {
    // Shift register pin
    uint8_t bit = pin - 200;
    if (state)
      shiftRegState |= (1 << bit);
    else
      shiftRegState &= ~(1 << bit);
    updateShiftRegister();
  }
  else {
    Serial.printf("GPIO_SET ERROR: unsupported pin range %d\n", pin);
  }
}

int GPIO_DIGITAL_READ(uint8_t pin) {
  if (!GPIO_IS_VALID(pin)) {
    Serial.printf("GPIO_DIGITAL_READ ERROR: invalid or reserved pin %d\n", pin);
    return LOW;
  }

  if (pin <= 10) {
    return digitalRead(pin);
  }
  else if (pin >= 100 && pin <= 107) {
    uint8_t ch = pin - 100;
    selectMuxChannel(ch);
    int val = analogRead(MUX_INPUT);
    return (val > 2000) ? HIGH : LOW;
  }
  else {
    Serial.printf("GPIO_DIGITAL_READ ERROR: unsupported pin %d\n", pin);
    return LOW;
  }
}

int GPIO_READ(uint8_t pin) {
  if (!GPIO_IS_VALID(pin)) {
    Serial.printf("GPIO_READ ERROR: invalid or reserved pin %d\n", pin);
    return 0;
  }

  if (pin <= 10) {
    return analogRead(pin);
  }
  else if (pin >= 100 && pin <= 107) {
    uint8_t ch = pin - 100;
    selectMuxChannel(ch);
    return analogRead(MUX_INPUT);
  }
  else {
    Serial.printf("GPIO_READ ERROR: unsupported pin %d\n", pin);
    return 0;
  }
}
