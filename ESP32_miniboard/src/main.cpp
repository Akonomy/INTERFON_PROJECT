#include <Arduino.h>
#include "network.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();


  // (opțional) sincronizare timp
  syncTime();


  // (opțional) primește comenzi din coadă
  receiveQueue();



   logSensorEvent(6, "system", "ESP32 boot completed", 2);     // info → syslog only
  logSensorEvent(5, "wifi", "Wi-Fi connected successfully", 2); // notice
  logSensorEvent(7, "diagnostics", "System entering idle mode", 2); // debug


  UPDATE_SENSOR("TEST_1", 1, "Door Open", "OK");

  // Example: log RFID access attempt
  LOG_ACCESS("A1B2C3D4", "2025-10-30 21:30:00", "granted", "Access granted to John");

  registerTAG("ABC123456789", "Test tag added via ESP32");
  deleteTAG("ABC123456789", "Lost card, remove access");

    checkTag("ABC123456789");
}

void loop() {
  // One-shot, ca viața la sesiune


}
