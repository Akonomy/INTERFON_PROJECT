#include <Arduino.h>
#include "network.h"
#include "control.h"
#include "mode_debug.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 DEBUG MODE START ===");

  connectToKnownWiFi();

  

  // Setup pins for testing (LED, relay, etc.)
  setupPin(2, true, LOW);  // Onboard LED
  setupPin(4, true, LOW);  // Example relay pin
}

void loop() {
  runDebugMode();
}
