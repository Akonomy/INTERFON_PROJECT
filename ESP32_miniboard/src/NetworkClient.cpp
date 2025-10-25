#include "NetworkClient.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "mbedtls/md.h"

NetworkClient::NetworkClient(const String& apiKey) : _apiKey(apiKey) {}

void NetworkClient::connectToKnownNetworks() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < MAX_NETWORKS; i++) {
        Serial.print("Trying: ");
        Serial.println(ssids[i]);

        WiFi.begin(ssids[i], passwords[i]);
        for (int retries = 0; retries < 10 && WiFi.status() != WL_CONNECTED; retries++) {
            delay(1000);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to WiFi.");
            return;
        }

        WiFi.disconnect(true);
    }

    Serial.println("Failed to connect to any known networks.");
}

bool NetworkClient::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkClient::computeHMAC(const String& key, const String& message) {
    byte hmac[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    char result[65];
    for (int i = 0; i < 32; i++) {
        sprintf(result + i * 2, "%02x", hmac[i]);
    }
    return String(result);
}

AuthResult NetworkClient::authenticate() {
    HTTPClient http;
    AuthResult auth = {false, ""};

    http.begin(_baseUrl + "/api/auth/challenge/");
    http.addHeader("Content-Type", "application/json");
    String body = "{\"key\":\"" + _apiKey + "\"}";
    int code = http.POST(body);
    String response = http.getString();
    http.end();

    StaticJsonDocument<512> doc;
    deserializeJson(doc, response);

    if (doc["status"] != "ok") return auth;

    String nonce = doc["nonce"];
    long ts = doc["ts"];
    int delaySecs = doc["min_delay_seconds"] | 2;
    String canonical = nonce + "|" + String(ts);

    delay(delaySecs * 1000);
    String hmac = computeHMAC(_apiKey, canonical);

    http.begin(_baseUrl + "/api/auth/respond/");
    http.addHeader("Content-Type", "application/json");
    body = "{\"key\":\"" + _apiKey + "\",\"nonce\":\"" + nonce + "\",\"response\":\"" + hmac + "\"}";
    code = http.POST(body);
    response = http.getString();
    http.end();

    deserializeJson(doc, response);
    if (doc["status"] != "ok") return auth;

    auth.success = true;
    auth.sessionToken = doc["session_token"];
    return auth;
}

TagCheckResult NetworkClient::checkTag(const String& sessionToken, const String& tagUid) {
    HTTPClient http;
    TagCheckResult result = {false, "none", ""};

    http.begin(_baseUrl + "/api/tag/check/");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Session-Token", sessionToken);
    String body = "{\"tag_uid\":\"" + tagUid + "\"}";
    int code = http.POST(body);
    String response = http.getString();
    http.end();

    StaticJsonDocument<512> doc;
    deserializeJson(doc, response);

    result.granted = doc["access_granted"];
    result.reason = doc["reason"] | "none";
    result.owner = doc["owner"] | "";

    return result;
}

bool NetworkClient::sendData(const String& endpoint, const String& json, String& responseOut) {
    HTTPClient http;
    http.begin(_baseUrl + endpoint);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(json);
    responseOut = http.getString();
    http.end();
    return code == 200;
}

bool NetworkClient::receiveData(const String& endpoint, String& responseOut) {
    HTTPClient http;
    http.begin(_baseUrl + endpoint);
    int code = http.GET();
    responseOut = http.getString();
    http.end();
    return code == 200;
}

void NetworkClient::sendSyslog(const String& message, const char* host, uint16_t port) {
    WiFiUDP udp;
    udp.beginPacket(host, port);
    udp.print("<134>ESP32: " + message);
    udp.endPacket();
}
