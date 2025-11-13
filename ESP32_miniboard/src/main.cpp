#include <Arduino.h>
#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include "rfid.h"
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
  if (inputLength >= MAX_INPUT_LEN) return; // block overrun

  char ch = KEYBOARD_READ_CHAR();

  if (ch == '#') {
    currentInput[inputLength] = '\0';

    if (inputLength > 0) {
      // Send the input as a log message
      logSensorEvent(6, "user", String(currentInput), 2); // info level, from "user"
      Serial.println("LOG SENT:");
      Serial.println(currentInput);
      OLED_DisplayText("LOG SENT");
    } else {
      OLED_DisplayText("EMPTY INPUT");
    }

    delay(2000);
    memset(currentInput, 0, sizeof(currentInput));
    inputLength = 0;
    OLED_Clear();
    return;
  }

  if (ch == '*') {
    if (inputLength > 0) {
      inputLength--;
      currentInput[inputLength] = '\0';
      updateDisplay();
    }
    return;
  }

  if (ch != '\0' && inputLength < MAX_INPUT_LEN) {
    currentInput[inputLength++] = ch;
    currentInput[inputLength] = '\0';
    updateDisplay();
  }





  rfid_tag_t tag;
  if (RFID_Read(&tag)) {
    Serial.printf("Tag UID: %08X\n", tag.uid);
    Serial.printf("Type: %d\n", tag.type);
    Serial.printf("Data: %s\n", tag.data);
    
}




}
