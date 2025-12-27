#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include "rfid.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_OFF = 0,
    LOG_MINIMAL = 1,
    LOG_FULL = 2
} LogLevel;

void log_set_level(LogLevel level);
LogLevel log_get_level(void);

typedef void (*log_output_fn)(const char *msg);
void log_set_output(log_output_fn fn);

// low-level implementations
void log_min_tag(const char *func, const char *msg);
void log_full_tag(const char *func, const char *msg);

void LogStuff();

// ───── friendly macros — auto add [FUNCTION] ─────
#define LOG_MIN(msg)  log_min_tag(__func__, msg)
#define LOG(msg)      log_full_tag(__func__, msg)



#ifdef __cplusplus
}
#endif

#endif
