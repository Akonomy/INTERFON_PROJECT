// network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

void connectWiFi(const char* ssid, const char* password);
bool authenticate();
void checkTag(const String& tagUID);
void sendData(const String& path, const String& jsonPayload);
String requestData(const String& path);
void sendSyslog(const String& message);
void syncTime();
void receiveQueue();

void connectAndCheckTag(const char* ssid = "Minerii din bomboclat", const char* password = "castravete", const String& tagUID = "SOME_TAG_UID");

#endif
