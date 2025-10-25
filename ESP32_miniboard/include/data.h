#pragma once
#include <ArduinoJson.h>

#define MAX_QUEUE 20

struct SensorData {
    String name;
    float value;
    String type; // "analog" or "digital"
};

struct AlertData {
    String level;
    String name;
    String description;
    String sensor_name;
    unsigned long timestamp;
};

extern SensorData sensorQueue[MAX_QUEUE];
extern int sensorQueueIndex;

extern AlertData alertQueue[MAX_QUEUE];
extern int alertQueueIndex;

void enqueueSensor(SensorData data);
void enqueueAlert(AlertData alert);
