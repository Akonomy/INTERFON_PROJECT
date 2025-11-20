#include <Arduino.h>
#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include "rfid.h"
#include <Wire.h>
#include "main.h"

#define MAX_INPUT_LEN 16




char currentInput[MAX_INPUT_LEN + 1]; // +1 for null terminator
int inputLength = 0;

void updateDisplay() {
  currentInput[inputLength] = '\0'; // make sure it’s null-terminated
  OLED_DisplayText(currentInput);
  Serial.print("Input: ");
  Serial.println(currentInput);
}




void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
  GPIO_INIT();
  OLED_Init();
  KEYBOARD_INIT();

  // (opțional) sincronizare timp
syncTime();
OLED_DisplayText("READY");

logSensorEvent(6, "system", "ESP32 boot completed", 2);     // info → syslog only

logSensorEvent(6, "diagnostics", "System entering DEBUG mode", 2); // debug


rfid_init();

  initServiceSequence();
logSensorEvent(6, "system", "ACCES CONTROL INITIALIZAT", 2); // debug


}






int currentMode = DEFAULT_START_MODE;

// =====================================================================
// SERVICE ENTRY SEQUENCE (3 coduri + 3 ENTER + timeout 2 min)
// =====================================================================
String serviceSteps[3] = { "123" ,"321" ,"111" };

int serviceStepIndex = 0;
int serviceEnterCount = 0;
bool serviceActive = false;
unsigned long serviceStartTime = 0;

void initServiceSequence()
{
    resetServiceSequence();
}

void resetServiceSequence()
{
    serviceStepIndex = 0;
    serviceEnterCount = 0;
    serviceActive = false;
}

void globalServiceMonitor(char* input)
{
    unsigned long now = millis();

    // timeout 2 minute
    if (serviceActive && (now - serviceStartTime > 60000)) {  // timeout 1 minut
        Serial.println("[SERVICE] Timeout → reset sequence");
        resetServiceSequence();
        return;
    }

    if (strlen(input) == 0)
        return;

    String val = String(input);

    // ENTER confirmation (input = "")
    if (val == "`" && serviceStepIndex == 3)
    {
        serviceEnterCount++;

        Serial.print("[SERVICE] ENTER press ");
        Serial.print(serviceEnterCount);
        Serial.println("/3");

        if (serviceEnterCount >= 3)
            enterServiceMode();

        return;
    }

    // checking the 3-step sequence
    if (serviceStepIndex < 3)
    {
        Serial.print("[SERVICE] Input received: ");
        Serial.println(val);

        if (val == serviceSteps[serviceStepIndex])
        {
            Serial.print("[SERVICE] Step ");
            Serial.print(serviceStepIndex + 1);
            Serial.println(" OK");

            if (!serviceActive) {
                serviceStartTime = now;
                Serial.println("[SERVICE] Starting service sequence timer");
            }

            serviceActive = true;
            serviceStepIndex++;

            if (serviceStepIndex == 3) {
                Serial.println("[SERVICE] All 3 codes OK. Waiting for 3×ENTER.");
                OLED_DisplayText("Press ENTER 3x");
            }

            return;
        }
        else
        {
            Serial.print("[SERVICE] WRONG code at step ");
            Serial.println(serviceStepIndex + 1);
            Serial.print("[SERVICE] Expected: ");
            Serial.println(serviceSteps[serviceStepIndex]);
            Serial.print("[SERVICE] Received: ");
            Serial.println(val);

            resetServiceSequence();
            return;
        }
    }
}


void enterServiceMode()
{
    currentMode = MODE_SERVICE;
    resetServiceSequence();

    OLED_Clear();
    OLED_DisplayText("SERVICE MODE");

}



// =====================================================================
// MODE SYSTEM
// =====================================================================
void setMode(int mode)
{
    if (mode < 0 || mode >= MAX_MODES)
        return;

    currentMode = mode;
    OLED_Clear();
}

void runCurrentMode()
{
    switch (currentMode)
    {
        case 0: mode_service(); break;
        case 1: mode1(); break;
        case 2: mode2(); break;
        case 3: mode3(); break;
        case 4: mode4(); break;
        case 5: mode5(); break;
        case 6: mode6(); break;
        case 7: mode7(); break;
        case 8: mode8(); break;
        case 9: mode9(); break;
        default:
            OLED_DisplayText("INVALID MODE");
    }
}



// =====================================================================
// MODE IMPLEMENTATIONS — toate în același fișier
// =====================================================================

// MODE 0 — SERVICE MODE
void mode_service()
{
    OLED_DisplayText("SERVICE");
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 0 - SERVICE");

    if (strlen(input) == 0)
        return;

    int target = atoi(input);

    if (target > 0 && target < MAX_MODES)
    {
        OLED_Clear();
        OLED_DisplayText("Loading MODE " + String(target));
        delay(400);

        setMode(target);

    }
    else
    {
        OLED_DisplayText("Invalid selection");
    }
}


// MODE 1
void mode1()
{
    OLED_DisplayText("MD1");
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 1 ACTIVE");

    // AICI scrii tu logica modului 1
    // Exemplu:
    // if (someCondition) doSomething();
}


// MODE 2
void mode2()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 2 ACTIVE");

    // logica modului 2
}


// MODE 3
void mode3()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 3 ACTIVE");
}


// MODE 4
void mode4()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 4 ACTIVE");
}


// MODE 5
void mode5()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 5 ACTIVE");
}


// MODE 6
void mode6()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 6 ACTIVE");
}


// MODE 7
void mode7()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 7 ACTIVE");
}


// MODE 8
void mode8()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 8 ACTIVE");
}


// MODE 9
void mode9()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 9 ACTIVE");
}



// =====================================================================
// MAIN LOOP
// =====================================================================



void loop()
{
    runCurrentMode();

}
