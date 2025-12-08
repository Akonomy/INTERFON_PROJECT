// network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <ArduinoJson.h>


#define DEVICE_ID "esp_test"
#define MAX_STORED_NETWORKS 10





// -----------------------------------------------------------------------------
//  STRUCTS
// -----------------------------------------------------------------------------
struct CommandResult {
  bool hasCommand = false;
  int queue_id = 0;
  int code = 0;
  int params[4] = {0, 0, 0, 0};
};

// -----------------------------------------------------------------------------
//  WIFI & AUTH
// -----------------------------------------------------------------------------
void connectWiFi(const char* ssid = "Minerii din bomboclat", const char* password = "castravete");
bool authenticate();
void syncTime();

// -----------------------------------------------------------------------------
//  API HELPERS
// -----------------------------------------------------------------------------
void sendData(const String& path, const String& jsonPayload);
String requestData(const String& path);

// -----------------------------------------------------------------------------
//  COMMANDS
// -----------------------------------------------------------------------------
CommandResult pollCommand();
void acknowledgeCommand(int queue_id);
void receiveQueue();

// -----------------------------------------------------------------------------
//  TAG OPERATIONS
// -----------------------------------------------------------------------------
uint8_t checkTag(const String& tagUID, const String& encryptedInfo) ;
void registerTAG(const String& tag_uid, const String& notes = "");
void deleteTAG(const String& tag_uid, const String& reason = "");
void connectAndCheckTag(const char* ssid = "Minerii din bomboclat", const char* password = "castravete", const String& tagUID = "SOME_TAG_UID");

String getTagInfo(const String& tagUID);

// -----------------------------------------------------------------------------
//  SENSOR FUNCTIONS
// -----------------------------------------------------------------------------
void UPDATE_SENSOR(const String& id_sensor,
                   float value_int,
                   const String& value_text,
                   const String& status);

// -----------------------------------------------------------------------------
//  ACCESS LOGGING
// -----------------------------------------------------------------------------
void LOG_ACCESS(const String& tag_uid,
                const String& esp_timestamp,
                const String& result,
                const String& details);

// -----------------------------------------------------------------------------
//  SYSLOG
// -----------------------------------------------------------------------------
void logSensorEvent(uint8_t code,
                    const String& sensorName,
                    const String& message,
                    uint8_t where=3);







// Struct to store BSSID (6 bytes) and RSSI
struct StoredNetwork {
  uint8_t bssid[6];  // MAC address
  int32_t rssi;      // Signal strength
};


extern StoredNetwork storedNetworks[MAX_STORED_NETWORKS];
extern uint8_t storedCount;




void scanAndStoreNetworks();
String getSSIDFromStoredBSSID(uint8_t bssid[6]);
#endif
