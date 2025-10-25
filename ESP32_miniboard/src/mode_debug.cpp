#include "mode_debug.h"
#include "network.h"
#include "control.h"
#include <HTTPClient.h>

unsigned long lastPoll = 0;
const int LED_PIN = 2;  // onboard LED for testing

void runDebugMode() {
  if (!isConnected()) {
    Serial.println("[DEBUG] Wi-Fi lost! Reconnecting...");
    connectToKnownWiFi();
    delay(1000);
    return;
  }

  if (millis() - lastPoll > 5000) {
    Serial.println("[DEBUG] Polling server for test commands...");
    pollServer();
    blinkPin(LED_PIN, 1, 100);
    lastPoll = millis();
  }
}

void pollServer() {
  HTTPClient http;
  String url = "http://akonomy.local/api/device/debug/";
  http.begin(url);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    Serial.printf("[DEBUG] Server response: %s\n", payload.c_str());
  } else {
    Serial.printf("[DEBUG] Poll failed: %d\n", code);
  }
  http.end();
}
