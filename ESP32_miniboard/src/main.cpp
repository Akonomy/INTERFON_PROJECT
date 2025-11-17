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
    registerTAG("TEST17112025", "Test tag added via ESP32");
}

void loop() {

    /**
    String uid, data;




    // CITIRe
    if (rfid_readTag(uid, data)) {
        Serial.println("\n--- TAG DETECTAT ---");
        Serial.print("UID: ");
        Serial.println(uid);
        Serial.print("Data RAW (string): ");
        Serial.println(data);
        Serial.println("-------------------");

        // TEST: scriem ceva nou
        String toWrite = "ACCESS_CODE_123456";
        Serial.print("✍ Scriu: ");
        Serial.println(toWrite);

        if (rfid_writeTag(toWrite)) {
            // citim din nou să verificăm
            delay(300);
            String uid2, data2;
            if (rfid_readTag(uid2, data2)) {
                Serial.print("🔁 Recitit: ");
                Serial.println(data2);
            }
        }

        Serial.println("\nApropie/indeparteaza tagul...");
        delay(1500);
    }

    delay(200);

    */


 

delay(5500);
/*
  Serial.println("\n: Requesting tag info...");
  String infoResponse = getTagInfo("TEST17112025");

  if (infoResponse == "") {
    Serial.println("❌ No response or API error");
    return;
  }

  // 5️⃣ Parse server response
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, infoResponse)) {
    Serial.println("❌ JSON parse error");
    return;
  }

  const char* status = doc["status"] | "error";

  if (strcmp(status, "expired") == 0) {
    Serial.println("⚠️ TAG INFO EXPIRED (older than 5 minutes)");
    return;
  }

  if (strcmp(status, "unknown") == 0) {
    Serial.println("❌ TAG does not exist");
    return;
  }

  if (strcmp(status, "ok") != 0) {
    Serial.printf("❌ Unexpected status: %s\n", status);
    return;
  }

  // 6️⃣ Extract encrypted info
  const char* encrypted_info = doc["encrypted_info"] | "";
  const char* owner = doc["owner"] | "N/A";

  Serial.println("\n=== TAG INFO RECEIVED ===");
  Serial.println("Owner          : " + String(owner));
  Serial.println("Encrypted Info : " + String(encrypted_info));
  Serial.println("Created At     : " + String(doc["created"].as<const char*>()));

*/
Serial.println("\n🔧 Testing checkTag()...");
uint8_t result = checkTag("TEST17112025","2025-11-17 23:35:24|17207b76ff");

if (result == 1) {
    Serial.println("✅ checkTag(): ACCESS GRANTED");
} else if (result == 0) {
    Serial.println("❌ checkTag(): ACCESS DENIED");
} else {
    Serial.println("⚠️ checkTag(): UNKNOWN TAG");
}

}


    



