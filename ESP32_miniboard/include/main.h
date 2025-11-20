#pragma once
#include <Arduino.h>

// -----------------------
// MODE SYSTEM
// -----------------------
#define MAX_MODES 10
#define MODE_SERVICE 0
#define DEFAULT_START_MODE MODE_SERVICE

extern int currentMode;

void setMode(int mode);
void runCurrentMode();


// -----------------------
// SERVICE ENTRY SEQUENCE
// -----------------------
void globalServiceMonitor(char* input);
void enterServiceMode();
void resetServiceSequence();
void initServiceSequence();


// -----------------------
// MODE FUNCTIONS
// -----------------------
void mode_service();
void mode1();
void mode2();
void mode3();
void mode4();
void mode5();
void mode6();
void mode7();
void mode8();
void mode9();

