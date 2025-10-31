// network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>


#define DEVICE_ID "esp_test"





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
void checkTag(const String& tagUID);
void registerTAG(const String& tag_uid, const String& notes = "");
void deleteTAG(const String& tag_uid, const String& reason = "");
void connectAndCheckTag(const char* ssid = "Minerii din bomboclat", const char* password = "castravete", const String& tagUID = "SOME_TAG_UID");
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

#endif
