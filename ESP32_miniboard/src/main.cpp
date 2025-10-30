#include <Arduino.h>
#include "network.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Conectare + autentificare + verificare tag
  connectAndCheckTag();

  // (opțional) sincronizare timp
  syncTime();

  // (opțional) trimite un log syslog-style
  sendSyslog("ESP32 boot complete");

  // (opțional) trimite niște date
  sendData("/api/data/post/", "{\"sensor\":\"temperature\",\"value\":22.5}");

  // (opțional) cere date
  String data = requestData("/api/data/latest/");
  Serial.println("Data received: " + data);

  // (opțional) primește comenzi din coadă
  receiveQueue();



   logSensorEvent(6, "system", "ESP32 boot completed", 2);     // info → syslog only
  logSensorEvent(5, "wifi", "Wi-Fi connected successfully", 2); // notice
  logSensorEvent(7, "diagnostics", "System entering idle mode", 2); // debug




}

void loop() {
  // One-shot, ca viața la sesiune

    CommandResult cmd = pollCommand();
    delay(2500);
}
