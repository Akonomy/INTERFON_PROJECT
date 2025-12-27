#ifndef GPIO_H
#define GPIO_H

#include <Arduino.h>
#include "log.h"


// ===========================
// Shift Register Outputs (Virtual Pins 200–207)
// ===========================

// 74HC595 OUTPUTS (write-only)
#define TAMPER_C   200
#define ROW_2      201
#define ROW_4      202
#define ROW_6      203
#define ROW_7      204
#define ACCESS_H   205
#define ACCESS_L   206
#define BUZZER     207

// ===========================
// MUX Inputs (Virtual Pins 100–107)
// ===========================

// CD4051 INPUTS (read-only)
#define COL_1      100
#define COL_3      101
#define COL_5      102
#define TAMPER_L   103
#define TAMPER_H   104
#define BATTERY_P  105


#define SDA_PIN 8
#define SCL_PIN 9

// Initialize all GPIO systems
void GPIO_INIT();

// Safe unified I/O functions
void GPIO_SET(uint8_t pin, bool state);
int  GPIO_DIGITAL_READ(uint8_t pin);
int  GPIO_READ(uint8_t pin);

// Check if a pin is valid (returns true if usable)
bool GPIO_IS_VALID(uint8_t pin);
float GPIO_CHECK_STATUS_AND_BATTERY();
void GPIO_CHECK_BATTERY(float* battery, uint8_t* tamper) ;
uint8_t GPIO_CHECK_TAMPER_FAST();
#endif
