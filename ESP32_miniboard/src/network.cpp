// network.cpp
#include "network.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <time.h>


#include <WiFiUdp.h>


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

CommandResult pollCommand() {
  CommandResult result;
  if (sessionToken == "") {
    Serial.println("⚠️ No session token available.");
    return result;
  }

  long ts = time(nullptr);
  if (ts == 0) {
    Serial.println("⚠️ Invalid timestamp.");
    return result;
  }

  String canonical = String(DEVICE_ID) + "|" + String(ts);
  String hmac = computeHMAC(API_KEY, canonical);

  HTTPClient http;
  http.begin(BASE_URL + "/api/commands/poll/");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> body;
  body["device"] = DEVICE_ID;
  body["timestamp"] = ts;
  body["signature"] = hmac;
  body["limit"] = 1;

  String payload;
  serializeJson(body, payload);

  int statusCode = http.POST(payload);
  String response = http.getString();
  http.end();

  if (statusCode != 200) {
    Serial.println("❌ Poll failed with status " + String(statusCode));
    return result;
  }

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, response)) {
    Serial.println("❌ Failed to parse poll JSON");
    return result;
  }

  JsonArray commands = doc["commands"];
  if (commands.size() == 0) {
    Serial.println("✅ No command available.");
    return result;
  }

  JsonObject cmd = commands[0];
  result.hasCommand = true;
  result.queue_id = cmd["queue_id"] | 0;
  result.code = cmd["code"] | 0;

  JsonArray params = cmd["params"];
  for (int i = 0; i < 4 && i < params.size(); i++) {
    result.params[i] = params[i] | 0;
  }

  Serial.println("\n📦 Command received:");
  Serial.println("  Queue ID: " + String(result.queue_id));
  Serial.println("  Code    : " + String(result.code));
  Serial.print("  Params  : ");
  for (int i = 0; i < 4; i++) {
    Serial.print(result.params[i]);
    if (i < 3) Serial.print(", ");
  }
  Serial.println();

  // ✅ AUTO-ACKNOWLEDGE
  {
    long ts_ack = time(nullptr);
    String statusTxt = "acknowledged";
    String canonicalAck = String(DEVICE_ID) + "|" + String(result.queue_id) + "|" + statusTxt + "|" + String(ts_ack);
    String sigAck = computeHMAC(API_KEY, canonicalAck);

    HTTPClient ack;
    ack.begin(BASE_URL + "/api/commands/ack/");
    ack.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> ackBody;
    ackBody["device"] = DEVICE_ID;
    ackBody["queue_id"] = result.queue_id;
    ackBody["timestamp"] = ts_ack;
    ackBody["signature"] = sigAck;
    ackBody["status"] = statusTxt;
    ackBody["detail"] = "Auto acknowledgment";

    String ackPayload;
    serializeJson(ackBody, ackPayload);

    int ackCode = ack.POST(ackPayload);
    String ackResponse = ack.getString();
    ack.end();

    if (ackCode == 200) {
      Serial.println("✅ ACK Sent! queue_id=" + String(result.queue_id));
    } else {
      Serial.println("❌ ACK failed: " + String(ackCode));
      Serial.println("Response: " + ackResponse);
    }
  }

  return result;
}



void acknowledgeCommand(int queue_id) {
  if (sessionToken == "") {
    Serial.println("⚠️ No session token available for acknowledgment.");
    return;
  }

  long ts = time(nullptr);
  if (ts == 0) {
    Serial.println("⚠️ Invalid timestamp for acknowledgment.");
    return;
  }

  String status = "acknowledged";
  String canonical = String(DEVICE_ID) + "|" + String(queue_id) + "|" + status + "|" + String(ts);
  String hmac = computeHMAC(API_KEY, canonical);

  HTTPClient http;
  http.begin(BASE_URL + "/api/commands/ack/");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> body;
  body["device"] = DEVICE_ID;
  body["queue_id"] = queue_id;
  body["timestamp"] = ts;
  body["signature"] = hmac;
  body["status"] = status;
  body["detail"] = "Executed successfully";

  String payload;
  serializeJson(body, payload);

  int code = http.POST(payload);
  String response = http.getString();
  http.end();

  if (code == 200) {
    Serial.println("✅ Command acknowledged: queue_id=" + String(queue_id));
  } else {
    Serial.println("❌ Failed to acknowledge command. Code: " + String(code));
    Serial.println("Response: " + response);
  }
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
  sendData("/api/syslog/", payload);
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






WiFiUDP udp;






const char* SYSLOG_SERVER = "akonomy.local";  // ← replace with your PC's IP
const int SYSLOG_PORT = 514;
const char* DEVICE_NAME = "esp32-core-01";    // ← can be static or generated

// Level mapping for human-readable labels
const char* severityLabel(int code) {
  switch (code) {
    case 0: return "EMERG";
    case 1: return "ALERT";
    case 2: return "CRIT";
    case 3: return "ERR";
    case 4: return "WARNING";
    case 5: return "NOTICE";
    case 6: return "INFO";
    case 7: return "DEBUG";
    default: return "UNKNOWN";
  }
}




void logSensorEvent(uint8_t code, const String& sensorName, const String& message, uint8_t where) {
  code = constrain(code, 0, 7);
  where = constrain(where, 1, 3);

  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S", t);

  // Syslog formatting (for Visual Syslog)
  int facility = 1; // user-level
  int pri = facility * 8 + code;
  String logLine = "<" + String(pri) + ">";
  logLine += String(timestamp) + " ";
  logLine += DEVICE_NAME;
  logLine += " " + sensorName + "[42]: " + message;

  // --- Send to SYSLOG (UDP) ---
  if (where == 1 || where == 2) {
    IPAddress syslogIP;
    if (!WiFi.hostByName(SYSLOG_SERVER, syslogIP)) {
      Serial.println("❌ Failed to resolve SYSLOG_SERVER hostname.");
    } else {
      udp.beginPacket(syslogIP, SYSLOG_PORT);
      udp.print(logLine);
      udp.endPacket();
      Serial.println("📡 Sent to SYSLOG: " + logLine);
    }
  }

  // --- Send to API ---
  if ((where == 2 || where == 3) && sessionToken != "") {

    int priority = code;  // facility (1 = user
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    StaticJsonDocument<256> json;

    json["severity"] = code;
    json["facility"] = "user";
    json["host"] = DEVICE_NAME;
    json["tag"] = sensorName + "[42]";
    json["message"] = message;
    json["priority"] = priority;
    json["device_time"] = String(timestamp);
    String payload;
    serializeJson(json, payload);
    sendData("/api/syslog/", payload);
    Serial.println("🌐 Sent to API: " + payload);
  } else if ((where == 2 || where == 3)) {
    Serial.println("⚠️ No session token. API log skipped.");
  }
}

void UPDATE_SENSOR(const String& id_sensor,
                   float value_int,
                   const String& value_text,
                   const String& status) {
  if (sessionToken == "") {
    Serial.println("⚠️ No session token. Sensor update skipped.");
    return;
  }

  StaticJsonDocument<256> json;
  json["device_id"] = DEVICE_ID;
  json["id_sensor"] = id_sensor;
  json["value_int"] = value_int;
  json["value_text"] = value_text;
  json["status"] = status;

  String payload;
  serializeJson(json, payload);

  sendData("/api/sensor/update/", payload);
  Serial.println("📤 Sensor update sent → " + id_sensor + ": " + status);
}



// ---------------------------------------------------------------------------
//  LOG_ACCESS — send access log entry (RFID or similar) to Django API
// ---------------------------------------------------------------------------
void LOG_ACCESS(const String& tag_uid,
                const String& esp_timestamp,
                const String& result,
                const String& details) {
  if (sessionToken == "") {
    Serial.println("⚠️ No session token. Access log skipped.");
    return;
  }

  StaticJsonDocument<256> json;
  json["device_id"] = DEVICE_ID;
  json["tag_uid"] = tag_uid;
  json["esp_timestamp"] = esp_timestamp;
  json["result"] = result;
  json["details"] = details;

  String payload;
  serializeJson(json, payload);

  sendData("/api/accesslog/", payload);
  Serial.println("📥 Access log sent → " + tag_uid + ": " + result);
}
