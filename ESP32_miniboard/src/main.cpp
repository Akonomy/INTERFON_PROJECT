#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include "rfid.h"
#include "main.h"

#define MAX_INPUT_LEN 16

// ════════════════════════════════════════════════════════════════
// 📌 GLOBAL STATE & CONFIGURATION
// ════════════════════════════════════════════════════════════════

bool canLog = true;
unsigned long lastLogTime = 0;
const unsigned long logInterval = 3000;  // legacy (unused)

int currentMode = DEFAULT_START_MODE;

char currentInput[MAX_INPUT_LEN + 1]; // +1 for null terminator
int inputLength = 0;

time_t lastLogEpoch = 0;
const unsigned long logIntervalSecs = 30;  // new log interval in seconds

// ───── Service Mode State ─────
String serviceSteps[3] = { "123", "321", "111" };
int serviceStepIndex = 0;
int serviceEnterCount = 0;
bool serviceActive = false;
unsigned long serviceStartTime = 0;

// ════════════════════════════════════════════════════════════════
// 🔧 UTILITY FUNCTIONS
// ════════════════════════════════════════════════════════════════

void blockLogs() {
    canLog = false;
    Serial.println("[LOG] Logging temporarily blocked");
}

void allowLogs() {
    canLog = true;
    Serial.println("[LOG] Logging allowed");
}

void updateDisplay() {
    currentInput[inputLength] = '\0'; // make sure it’s null-terminated
    OLED_DisplayText(currentInput);
    Serial.print("Input: ");
    Serial.println(currentInput);
}

// ════════════════════════════════════════════════════════════════
// 📝 LOGGING FUNCTION
// ════════════════════════════════════════════════════════════════

void LogStuff() {
    if (!canLog) return;

    time_t now = time(nullptr);
    if (now == 0) return;

    if ((now - lastLogEpoch) < 6 0) return;
    lastLogEpoch = now;

    int rssi = WiFi.RSSI();  // Wi-Fi signal strength (dBm)
    unsigned long uptime = millis() / 1000;

    // LOW SIGNAL → minimal logging + higher priority
    if (rssi < -75) {
        char msg[64];
        snprintf(msg, sizeof(msg),
            "[WIFI-LOW] RSSI=%d dBm | Uptime=%lus", rssi, uptime);

        int severity = (rssi < -85) ? 2 : 4;  // 2 = critical, 4 = warning
        logSensorEvent(severity, "wifi", msg, 2);
        return;
    }

    // GOOD SIGNAL → log full system info
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t heapSize = ESP.getHeapSize();
    uint32_t freePsram = ESP.getFreePsram();
    const char* resetReason = esp_reset_reason() == ESP_RST_POWERON ? "PowerOn" : "Other";

    char msg[160];
    snprintf(msg, sizeof(msg),
        "[SYS] RSSI=%d dBm | Heap=%lu/%lu | Min=%lu | PSRAM=%lu | Uptime=%lus | Reset=%s",
        rssi, freeHeap, heapSize, minFreeHeap, freePsram, uptime, resetReason);

    logSensorEvent(6, "system", msg, 2);  // 6 = info
}


// ════════════════════════════════════════════════════════════════
// 🔧 SERVICE MODE LOGIC
// ════════════════════════════════════════════════════════════════

void initServiceSequence() {
    resetServiceSequence();
}

void resetServiceSequence() {
    serviceStepIndex = 0;
    serviceEnterCount = 0;
    serviceActive = false;
}

void globalServiceMonitor(char* input) {
    if (strchr(input, '@') != NULL) {
        Serial.println("[SERVICE] Invalid input — '@' is not allowed. Skipping service monitor.");
        resetServiceSequence();
        return;
    }

    unsigned long now = millis();

    if (serviceActive && (now - serviceStartTime > 60000)) {
        resetServiceSequence();
        return;
    }

    if (strlen(input) == 0)
        return;

    String val = String(input);

    if (val == "`" && serviceStepIndex == 3) {
        serviceEnterCount++;
        if (serviceEnterCount >= 3)
            enterServiceMode();
        return;
    }

    if (serviceStepIndex < 3) {
        if (val == serviceSteps[serviceStepIndex]) {
            if (!serviceActive) {
                serviceStartTime = now;
            }
            serviceActive = true;
            serviceStepIndex++;
            return;
        } else {
            Serial.print("[SERVICE] WRONG code at step ");
            Serial.println(serviceStepIndex + 1);
            resetServiceSequence();
            return;
        }
    }
}

void enterServiceMode() {
    currentMode = MODE_SERVICE;
    resetServiceSequence();
    OLED_Clear();
    OLED_DisplayText("SERVICE MODE");
}

// ════════════════════════════════════════════════════════════════
// 🚀 SETUP FUNCTION
// ════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(500);

    connectWiFi("C004","53638335");
    GPIO_INIT();
    OLED_Init();
    KEYBOARD_INIT();
    rfid_init();

    syncTime();

    logSensorEvent(6, "system", "ESP32 boot completed", 2);

    initServiceSequence();

    OLED_DisplayText("READY");
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
    LogStuff();
    OLED_DisplayText("SERVICE");
    char* input = KEYBOARD_READ(0);

    // ───── Skip special control returns ─────
    if (strcmp(input, "@TIME") == 0 || strcmp(input, "@NONE") == 0) {

        return;
    }

    if (strlen(input) == 0)
        return;


    globalServiceMonitor(input);


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
    LogStuff();
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

    // ignorăm ENTER gol sau timeout
    if ((strlen(input) == 0) || (strchr(input, '@') != NULL)) goto start_over;





    // transformăm în număr
    uint32_t pin_entered = strtoul(input, NULL, 10);

    // parola ta
    const uint32_t MODE1_PASSWORD = 999999;

    // verificare silent
    if (pin_entered == MODE1_PASSWORD){
        pin_correct = true;
         Serial.println("PASS OK");
}
    else {
        pin_correct = false;
         Serial.println("PASS WRONG");
    }
    // -------------------------------+
    // PASUL 2 — CITIRE TAG RFID
    // -------------------------------
    OLED_Clear();
    OLED_DisplayText("SCAN TAG");

    uid = "";
    data = "";

    // blocăm până detectăm un tag
    unsigned long startTime = millis();
unsigned long timeout = 10000; //  seconds timeout10

// blocăm până detectăm un tag sau până expiră timpul
while (uid.length() == 0 && (millis() - startTime < timeout))
{
    rfid_readTag(uid, data);
    delay(100); // gentle pause so your MCU doesn’t catch fire
}

if (uid.length() == 0)
{
    // Tag not detected within timeout
    Serial.println("Timeout: No RFID tag detected.");
    tag_correct = false;
}
else
{
    // Tag detected
    Serial.print("Tag detected: ");
    Serial.println(uid);



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
    OLED_Clear();
    OLED_DisplayText("enter to register");

    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);
    String confirm = String(input);





    if(confirm=="`"){
Serial.println("CONFIRMAT PAS 1");
OLED_Clear();
rfid_readTag(uid, data);



Serial.println(uid);
registerTAG(uid, "Test tag added via ESP32");
Serial.println(confirm);
 OLED_DisplayText("INREGISTRARE");
Serial.println("15 SEC FROM NOW");
delay(15000);

 Serial.println("\n: Requesting tag info...");
    String infoResponse = getTagInfo(uid);
Serial.println(infoResponse);


  if (infoResponse == "") {
    Serial.println("❌ No response or API error");
    return;
  }
  OLED_Clear();
 OLED_DisplayText("RFID+ENTER");


OLED_DisplayText("DO NOT REMOVE TAG!");
 String tagWriteData = serverToTagPlaintext(infoResponse);

 rfid_writeTag(tagWriteData);


    }
 //end confirm registered tag
Serial.println("END");


}  //end module 2


// MODE 3
void mode3()
{
    LogStuff();
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 3 ACTIVE");
}


// MODE 4
void mode4()
{
    LogStuff();
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
