#include <Arduino.h>
#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include <Wire.h>

#define MAX_INPUT_LEN 16

char currentInput[MAX_INPUT_LEN + 1]; // +1 for null terminator
int inputLength = 0;

void updateDisplay() {
  currentInput[inputLength] = '\0'; // make sure it’s null-terminated
  OLED_DisplayText(currentInput);
  Serial.print("Input: ");
  Serial.println(currentInput);
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
  GPIO_INIT();
  OLED_Init();
  KEYBOARD_INIT();


  // (opțional) sincronizare timp
  syncTime();
    OLED_DisplayText("READY");


  // (opțional) primește comenzi din coadă
  receiveQueue();



   logSensorEvent(6, "system", "ESP32 boot completed", 2);     // info → syslog only
  logSensorEvent(5, "wifi", "Wi-Fi connected successfully", 2); // notice
  logSensorEvent(7, "diagnostics", "System entering DEBUG mode", 2); // debug

/*
  UPDATE_SENSOR("TEST_1", 1, "Door Open", "OK");

  // Example: log RFID access attempt
  LOG_ACCESS("A1B2C3D4", "2025-10-30 21:30:00", "granted", "Access granted to John");

  registerTAG("ABC123456789", "Test tag added via ESP32");
  deleteTAG("ABC123456789", "Lost card, remove access");

    checkTag("ABC123456789");

    */
    memset(currentInput, 0, sizeof(currentInput));
     OLED_Clear();
}

void loop() {
  KeyEvent key = KEYBOARD_READ_KEY();

  if (key.type == KEY_CHAR) {
    if (inputLength < MAX_INPUT_LEN) {
      currentInput[inputLength++] = key.value;
      updateDisplay();
    }
  } else if (key.type == KEY_CLEAR) {
    if (inputLength > 0) {
      inputLength--;
      currentInput[inputLength] = '\0';
      updateDisplay();
    }
  } else if (key.type == KEY_ENTER) {
    OLED_DisplayText("DONE");
    Serial.println("DONE");
    delay(2000);
    memset(currentInput, 0, sizeof(currentInput));
    inputLength = 0;
    OLED_Clear();
  }
}
