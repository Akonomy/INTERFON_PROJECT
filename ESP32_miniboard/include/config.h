#pragma once
#include <Arduino.h>

#define DEVICE_ID "INTERFON_01"
#define SERVER_URL "http://esp32server.local:5000/api/senzor"

#define LED_PIN 2
#define RELAY_PIN 4
#define SENSOR_PIN 34

void config_init();
