// network.cpp
#include "network.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <time.h>

static const String BASE_URL = "http://akonomy.local:8000";
static const String API_KEY = "f3109dd8c54f3ab0798c6b79756e404ed2ee350ee4b1cd9c2a7f4be40c59c57e";
static String sessionToken = "";

static String computeHMAC(String key, String message) {
  byte hmac[32];
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  char hex_result[65];
  for (int i = 0; i < 32; i++) {
    sprintf(hex_result + i * 2, "%02x", hmac[i]);
  }
  return String(hex_result);
}

void connectWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
}

bool authenticate() {
  HTTPClient http;
  Serial.println("→ Requesting challenge...");
  http.begin(BASE_URL + "/api/auth/challenge/");
  http.addHeader("Content-Type", "application/json");
  String body = "{\"key\":\"" + API_KEY + "\"}";
  int code = http.POST(body);
  String response = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  deserializeJson(doc, response);
  if (doc["status"] != "ok") {
    Serial.println("⚠️ Challenge rejected.");
    return false;
  }

  String nonce = doc["nonce"];
  long ts = doc["ts"];
  int delaySecs = doc["min_delay_seconds"] | 2;
  String canonical = nonce + "|" + String(ts);

  Serial.printf("✔️ Got nonce. Waiting %ds...\n", delaySecs);
  delay(delaySecs * 1000);

  String hmac = computeHMAC(API_KEY, canonical);
  Serial.println("→ Sending HMAC response...");
  http.begin(BASE_URL + "/api/auth/respond/");
  http.addHeader("Content-Type", "application/json");
  String rbody = "{\"key\":\"" + API_KEY + "\",\"nonce\":\"" + nonce + "\",\"response\":\"" + hmac + "\"}";
  code = http.POST(rbody);
  response = http.getString();
  http.end();

  deserializeJson(doc, response);
  if (doc["status"] != "ok") {
    Serial.println("❌ Authentication failed.");
    return false;
  }

  sessionToken = doc["session_token"].as<String>();
  Serial.println("✅ Auth success. Session: " + sessionToken);
  return true;
}

void checkTag(const String& tagUID) {
  if (sessionToken == "") return;
  HTTPClient http;
  Serial.println("→ Checking tag...");
  http.begin(BASE_URL + "/api/tag/check/");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Session-Token", sessionToken);
  String tagBody = "{\"tag_uid\":\"" + tagUID + "\"}";
  int code = http.POST(tagBody);
  String response = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  deserializeJson(doc, response);
  bool granted = doc["access_granted"];
  const char* reason = doc["reason"] | "none";

  Serial.printf("🚪 Access: %s\n", granted ? "GRANTED 🎉" : "DENIED 🔒");
  Serial.println("Reason: " + String(reason));
}

void sendData(const String& path, const String& jsonPayload) {
  if (sessionToken == "") return;
  HTTPClient http;
  http.begin(BASE_URL + path);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Session-Token", sessionToken);
  int code = http.POST(jsonPayload);
  String response = http.getString();
  http.end();
  Serial.println("→ Sent data: " + String(code));
  Serial.println(response);
}

String requestData(const String& path) {
  if (sessionToken == "") return "";
  HTTPClient http;
  http.begin(BASE_URL + path);
  http.addHeader("X-Session-Token", sessionToken);
  int code = http.GET();
  String response = http.getString();
  http.end();
  Serial.println("→ Requested data: " + String(code));
  return response;
}

void sendSyslog(const String& message) {
  String payload = "{\"log\":\"" + message + "\"}";
  sendData("/api/log/syslog/", payload);
}

void syncTime() {
  configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to obtain time");
    return;
  }

  Serial.printf("🕒 Local time: %s", asctime(&timeinfo));
}

void receiveQueue() {
  String response = requestData("/api/data/queue/");
  if (response == "") return;

  StaticJsonDocument<1024> doc;
  deserializeJson(doc, response);
  JsonArray queue = doc["queue"];

  for (JsonObject item : queue) {
    String cmd = item["command"];
    String payload = item["payload"];
    Serial.printf("📥 Received: %s | Payload: %s\n", cmd.c_str(), payload.c_str());
    // aici faci ceva cu comanda
  }
}

void connectAndCheckTag(const char* ssid, const char* password, const String& tagUID) {
  connectWiFi(ssid, password);
  if (authenticate()) {
    checkTag(tagUID);
  }
}
