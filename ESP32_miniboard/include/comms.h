#pragma once
#include <ArduinoJson.h>
#include "data.h"

void sendSensorData(SensorData data);
void sendAlert(AlertData alert);
void requestControl();
