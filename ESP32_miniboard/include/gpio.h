#ifndef GPIO_H
#define GPIO_H

#include <Arduino.h>

// Initialize all GPIO systems
void GPIO_INIT();

// Safe unified I/O functions
void GPIO_SET(uint8_t pin, bool state);
int  GPIO_DIGITAL_READ(uint8_t pin);
int  GPIO_READ(uint8_t pin);

// Check if a pin is valid (returns true if usable)
bool GPIO_IS_VALID(uint8_t pin);

#endif
