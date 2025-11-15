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




    rfid_init();
}

void loop() {

    if (rfid_detect()) {
        Serial.println("\n--- TAG DETECTAT ---");
        rfid_printUID();

        // Dacă este NTAG sau Ultralight
        if (rfid_tag.type != RFID_TYPE_MIFARE_1K) {
            Serial.println("⚠ Tag NU este MIFARE Classic → nu are blocuri de 16 bytes.\n");
            delay(1000);
            return;
        }

        // Dacă este MIFARE Classic 1K → citim blocul 4
        Serial.println("🔍 Citim blocul 4 (user data)...");
        uint8_t blockData[16];

        if (rfid_readBlock(4, blockData)) {
            Serial.print("📖 Bloc 4: ");
            for (int i = 0; i < 16; i++) {
                delay(10);
                if (blockData[i] < 0x10) Serial.print("0");
                Serial.print(blockData[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

        } else {
            Serial.println("❌ Citirea blocului a eșuat.");
        }

        // Scriem un test în blocul 4
        Serial.println("✍ Scriem text de test în blocul 4...");
        uint8_t writeTest[16] = {
            'T','E','S','T','_','D','A','T','A','_',0x01,0x02,0x03,0x04,0x05,0x06
        };
        delay(1000);

        if (rfid_writeBlock(4, writeTest)) {
            Serial.println("✅ Scriere OK");
        } else {
            Serial.println("❌ Scriere eșuată.");
        }

        delay(500);

        // Recitim pentru confirmare
        Serial.println("🔁 Recitim blocul 4...");
        if (rfid_readBlock(4, blockData)) {
            Serial.print("📖 Bloc 4 dupa scriere: ");
            for (int i = 0; i < 16; i++) {
                if (blockData[i] < 0x10) Serial.print("0");
                Serial.print(blockData[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            rfid_printDecodedBlock(blockData);
        }

        Serial.println("\n---------------------------\n");
        delay(1500);
    }

    delay(200);
}

