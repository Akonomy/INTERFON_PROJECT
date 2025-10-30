// network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>


#define DEVICE_ID 1




void connectWiFi(const char* ssid, const char* password);
bool authenticate();
void checkTag(const String& tagUID);
void sendData(const String& path, const String& jsonPayload);
String requestData(const String& path);
void sendSyslog(const String& message);
void syncTime();
void receiveQueue();

void connectAndCheckTag(const char* ssid = "Minerii din bomboclat", const char* password = "castravete", const String& tagUID = "SOME_TAG_UID");



// ---------- NEW STUFF ----------

// Struct to hold command result
struct CommandResult {
  bool hasCommand = false;
  int queue_id = 0;
  int code = 0;
  int params[4] = {0, 0, 0, 0};
};





// Function that polls for a command and auto-acknowledges it
CommandResult pollCommand();
void logSensorEvent(uint8_t code, const String& sensorName, const String& message, uint8_t where = 3);


#endif
