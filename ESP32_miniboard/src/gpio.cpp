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
  delayMicroseconds(32);
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
    delay(5);
    int val = analogRead(MUX_INPUT);
    return (val > 1000) ? HIGH : LOW;
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
    delay(5);
    return analogRead(MUX_INPUT);
  }
  else {
    Serial.printf("GPIO_READ ERROR: unsupported pin %d\n", pin);
    return 0;
  }
}



float GPIO_CHECK_STATUS_AND_BATTERY() {
  // Step 1: Activate tamper check
  GPIO_SET(TAMPER_C, true);

  // Wait 2ms (unfortunately, precision rituals need time)
  delay(2);

  // Step 2: Read tamper pins
  bool tamperL = GPIO_DIGITAL_READ(TAMPER_L);
  bool tamperH = GPIO_DIGITAL_READ(TAMPER_H);

  // Tamper logic: compromised if L is HIGH and H is LOW
  bool tamperCompromised = (tamperL == HIGH && tamperH == LOW);

  // Step 3: Read battery voltage
    int adcVal = GPIO_READ(BATTERY_P); // 0–4095

    // Calibrated scale factor (0 -> 0 V), based on 3 measured points
    const float k = 0.002315f;

    float vBat = k * (float)adcVal;





  // Round to 1 decimal place
  vBat = roundf(vBat * 10.0f) / 10.0f;

  // Step 4: Return final code
  if (tamperCompromised) {
    return 100.0f + vBat;
  } else {
    return 200.0f + vBat;
  }
}


void GPIO_CHECK_BATTERY(float* battery, uint8_t* tamper) {
    if (!battery || !tamper) return; // safety check

    // Step 1: Activate tamper check
    GPIO_SET(TAMPER_C, true);

    // Wait 2ms for stable reading
    delay(2);

    // Step 2: Read tamper pins
    bool tamperL = GPIO_DIGITAL_READ(TAMPER_L);
    bool tamperH = GPIO_DIGITAL_READ(TAMPER_H);

    // Tamper logic: compromised if L is HIGH and H is LOW
    *tamper = (tamperL == HIGH && tamperH == LOW) ? 1 : 0;

    // Step 3: Read battery voltage
    int adcVal = GPIO_READ(BATTERY_P); // 0–4095

    // Calibrated scale factor (0 -> 0 V), based on 3 measured points
    const float k = 0.002315f;

    float vBat = k * (float)adcVal;

    *battery = roundf(vBat * 10.0f) / 10.0f;

    // Round to 1 decimal place
    *battery = roundf(vBat * 10.0f) / 10.0f;
}




uint8_t GPIO_CHECK_TAMPER_FAST() {
  // Enable tamper circuit
  GPIO_SET(TAMPER_C, true);

  // Check HIGH side FIRST (wire cut = LOW)
  int tamperH = GPIO_DIGITAL_READ(TAMPER_H);
  if (tamperH != HIGH) {
    return 1; // compromised immediately
  }

  // Then check LOW side
  int tamperL = GPIO_DIGITAL_READ(TAMPER_L);
  if (tamperL != LOW) {
    return 1; // compromised
  }

  return 0; // tamper OK
}


