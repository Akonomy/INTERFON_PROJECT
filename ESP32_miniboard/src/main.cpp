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
  //connectWiFi("C004", "53638335");
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
        case 1: mode1(); break;  //mode public
        case 2: mode2(); break;  //mode  register tag
        case 3: mode3(); break;  //mode out of service
        case 4: mode4(); break; //mode connect to wifi new
        case 5: mode5(); break; //mode log a msg
        case 6: mode6(); break; //mode show info about a card
        case 7: mode7(); break;
        case 8: mode8(); break;
        case 9: mode9(); break;
        default:
            OLED_DisplayText("INVALID MODE");
    }
}



// =====================================================================
// MODE IMPLEMENTATIONS —
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
    // -------------------------------
    // VARIABILE LOCALE
    // -------------------------------
    String uid, data;
    bool pin_correct = false;
    bool tag_correct = false;

    // -------------------------------
    // PASUL 1 — INTRODUCERE PIN
    // -------------------------------
start_over:

    pin_correct = false;
    tag_correct = false;

    OLED_Clear();
    OLED_DisplayText("ENTER PIN");

    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    // ignorăm ENTER gol
    if (strlen(input) == 0) goto start_over;

    // transformăm în număr
    uint32_t pin_entered = strtoul(input, NULL, 10);

    // parola ta
    const uint32_t MODE1_PASSWORD = 9999999;

    // verificare silent
    if (pin_entered == MODE1_PASSWORD)
        pin_correct = true;
    else
        pin_correct = false;

    // -------------------------------
    // PASUL 2 — CITIRE TAG RFID
    // -------------------------------
    OLED_Clear();
    OLED_DisplayText("SCAN TAG");

    uid = "";
    data = "";

    // blocăm până detectăm un tag
    while (uid.length() == 0)
    {
        rfid_readTag(uid, data);
        delay(100);
    }

    // parsează tag-ul
    ParsedTagData parsed = parsePlaintext(data);

    if (!parsed.valid)
    {
        Serial.println("❌ Tag format invalid!");
        tag_correct = false;
    }
    else
    {
        String encrypted_info = parsed.encrypted_info;
        uint8_t result = checkTag(uid, encrypted_info);

        // doar serial debug, NU afișăm la user
        if (result == 1)
        {
            Serial.println("✅ checkTag(): ACCESS GRANTED");
            tag_correct = true;
        }
        else if (result == 0)
        {
            Serial.println("❌ checkTag(): ACCESS DENIED");
            tag_correct = false;
        }
        else
        {
            Serial.println("⚠️ checkTag(): UNKNOWN TAG");
            tag_correct = false;
        }
    }

    // -------------------------------
    // PASUL 3 — VERDICT FINAL
    // -------------------------------
    OLED_Clear();

    if (pin_correct && tag_correct)
    {
        // ✔✔ AMBELE OK
        OLED_DisplayText("413");
        delay(2000);

        // aici intră logica normală a modului 1
        OLED_Clear();
        OLED_DisplayText("MODE 1 ACTIVE");

        Serial.println("[MODE1] ACCESS GRANTED (PIN+TAG)");
        return;   // gata, modul rămâne activ
    }
    else
    {
        // ❌ cel putin un factor e gresit
        OLED_DisplayText("REJECTED");
        delay(500);
        goto start_over;   // revine la începutul modului (PIN + TAG)
    }
}


// MODE 2
void mode2()
{
    String uid, data;
    OLED_DisplayText("enter to register");

    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);
    String confirm = String(input);
    rfid_readTag(uid, data);

    registerTAG(uid, "Test tag added via ESP32");






    if(confirm=="`"){




 Serial.println("\n: Requesting tag info...");
  String infoResponse = getTagInfo("TEST17112025");



  if (infoResponse == "") {
    Serial.println("❌ No response or API error");
    return;
  }



 String tagWriteData = serverToTagPlaintext(infoResponse);

 rfid_writeTag(tagWriteData);


    } //end confirm registered tag


}  //end module 2


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
