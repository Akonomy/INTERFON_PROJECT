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
}

void loop() {
  // One-shot, ca viața la sesiune
}
