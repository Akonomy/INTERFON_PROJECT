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


#define MAX_NETWORKS 10


uint8_t wifiSortedIndices[MAX_NETWORKS];
uint8_t wifiNetworkCount = 0;
int8_t wifiSelectedIndex = 0;
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
void tamperTask(void *param) {
  while (true) {
    uint8_t tamper = GPIO_CHECK_TAMPER_FAST();

    // Do something smart (or dumb) with the result
    Serial.print("[TAMPER TASK] Status: ");
    Serial.println(tamper);

    // Wait 5 seconds without blocking other tasks
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}



void LogStuff() {
    if (!canLog) return;

    time_t now = time(nullptr);
    if (now == 0) return;

    if ((now - lastLogEpoch) < 420) return;
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

    if (serviceActive && (now - serviceStartTime > 15000)) {
        resetServiceSequence();
        return;
    }

    if (strlen(input) == 0)
        return;

    String val = String(input);

    Serial.println(serviceStepIndex);

    if (val == "`" && serviceStepIndex >= 2) {
        Serial.print("[SERVICE]>>>");
        serviceEnterCount++;
         Serial.print(serviceEnterCount);
         Serial.println("[SERVICE]>>>");
        if (serviceEnterCount >= 2)

            enterServiceMode();
            Serial.print("[SERVICE]>>SUCCES");
        return;
    }

    if (serviceStepIndex < 3) {
        Serial.print(">>>[SERVICE]");
           Serial.print(serviceSteps[serviceStepIndex]);
              Serial.println(">>>[SERVICE]");
        if (val == serviceSteps[serviceStepIndex]) {
            if (!serviceActive) {
                serviceStartTime = now;
            }
            serviceActive = true;
            serviceStepIndex++;
            return;
        } else {
            Serial.print("[SERVICE] received : ");
            Serial.println(val);
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

    //connectWiFi("C004","53638335");
    connectWiFi("Minerii din bomboclat","castravete");
    GPIO_INIT();
    OLED_Init();
    KEYBOARD_INIT();
    rfid_init();

    syncTime();

/*      // Create tamper task on core 0, priority 1
  xTaskCreatePinnedToCore(
    tamperTask,
    "TamperCheckTask",
    2048,     // Stack size in words, not bytes
    NULL,
    1,        // Priority (low, so it doesn't mess with Wi-Fi etc)
    NULL,
    0         // Core 0 (leave core 1 to Wi-Fi / heavy stuff)
  );

*/

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
    monitorServerStatus(currentMode);

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

void mode1()
{
    LogStuff();
    String uid, data;

start_over:

    bool pin_correct = false;
    bool tag_correct = false;
    unsigned long idleStart = millis();

    OLED_Clear();
    OLED_DisplayText("ENTER PIN");

    while (millis() - idleStart < 15000)
    {
        char* input = KEYBOARD_READ(0);


        if (strcmp(input, "@") == 0 || strlen(input) == 0) goto start_over;

        // ─── Entered "1" → possible service entry ───
        if (strcmp(input, "1") == 0)
        {
            OLED_Clear();
            OLED_DisplayText("SCAN TAG...");
            delay(5000); // fake wait
            OLED_Clear();
            OLED_DisplayText("WAITING...");

            // Now wait for Enter to begin service sequence
            while (true) {
                char* in = KEYBOARD_READ(0);

                if (strcmp(in, "@") == 0) continue;
                if (strcmp(in, "`") == 0) break;

                // Any other input = restart
                goto start_over;
            }

            // Start collecting service codes
            for (int i = 0; i < 6; i++) {
                OLED_Clear();
                OLED_DisplayText("SCAN TAG...");
                delay(600);

                char* stepInput = KEYBOARD_READ(0);
                globalServiceMonitor(stepInput);

                // Fake error to mask service input
                const char* fakeErr[] = {
                    "TAG FAIL", "DECRYPT ERR", "CALL SERVICE",
                    "CORRUPT TAG", "RFID TIMEOUT", "UNKNOWN TAG"
                };
                OLED_Clear();
                OLED_DisplayText(fakeErr[random(0, 6)]);
                delay(800);
            }

            // After 6 fake steps, restart loop in case service failed
            goto start_over;
        }

        // ─── Normal PIN entry ───
        uint32_t pin = strtoul(input, NULL, 10);
        if (pin == 999999) {
            pin_correct = true;
        }
        break;
    }

    // ==============================
    // RFID SCAN STEP
    // ==============================
    OLED_Clear();
    OLED_DisplayText("SCAN TAG");

    uid = "";
    data = "";
    unsigned long scanStart = millis();

    while (millis() - scanStart < 10000)
    {
        rfid_readTag(uid, data);

        if (uid.length() > 0) break;

        char* input = KEYBOARD_READ(0);


        if (strcmp(input, "@") != 0 && strcmp(input, "`") != 0)
            goto  start_over;

        delay(100);
    }

    if (uid.length() == 0) {
        OLED_Clear();
        OLED_DisplayText("TAG FAIL");
        delay(1000);
        goto start_over;
    }

    // ==============================
    // PARSE TAG & CHECK
    // ==============================
    ParsedTagData parsed = parsePlaintext(data);
    if (!parsed.valid) {
        OLED_Clear();
        OLED_DisplayText("TAG ERROR");
        delay(1000);
        goto start_over;
    }

    if (checkTag(uid, parsed.encrypted_info) == 1)
        tag_correct = true;

    // ==============================
    // FINAL RESULT
    // ==============================
    OLED_Clear();
    if (pin_correct && tag_correct)
    {
        OLED_DisplayText("413");
        delay(1500);
        OLED_Clear();
        OLED_DisplayText("MODE 1 ACTIVE");
        Serial.println("[MODE1] ACCESS GRANTED");
        return;
    }
    else
    {
        OLED_DisplayText("REJECTED");
        delay(1000);
    }



        Serial.print("f _mode1_ ");
}


void mode2()
{
    String uid, data;
    OLED_Clear();
    OLED_DisplayText("Press ENTER to start");

    // Wait for ENTER to start
    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        if (String(input) == "`") break;
    }

read_tag_again:

    OLED_Clear();
    OLED_DisplayText("Approach tag...");

    // Try reading the RFID tag
    bool tagReadSuccess = rfid_readTag(uid, data);

    while (!tagReadSuccess) {
        OLED_Clear();
        OLED_DisplayText("Tag not found\n1-Service\n2-Retry");

        while (true) {
            char* input = KEYBOARD_READ(0);
            globalServiceMonitor(input);
            String choice = String(input);

            if (choice == "1") {
                Serial.println("[MODE2] Switching to SERVICE MODE...");
                enterServiceMode();
                return;
            } else if (choice == "2") {
                OLED_Clear();
                OLED_DisplayText("Retrying...\nApproach tag");
                tagReadSuccess = rfid_readTag(uid, data);
                if (tagReadSuccess) break;
            }
        }
    }

    Serial.println("[MODE2] Tag detected:");
    Serial.println(uid);
    OLED_Clear();
    OLED_DisplayText("Tag OK\nPress ENTER");

    // Confirm before registering tag
    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        if (String(input) == "`") break;
    }

    registerTAG(uid, "Test tag added via ESP32");
    Serial.println("[MODE2] Tag registered");

    OLED_Clear();
    OLED_DisplayText("Registered\nPress ENTER");

    // Confirm before next step
    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        if (String(input) == "`") break;
    }

    Serial.println("[MODE2] Requesting tag info from server...");
    String infoResponse = getTagInfo(uid);
    Serial.println(infoResponse);

    if (infoResponse == "") {
        Serial.println("❌ API error: No response");
        OLED_Clear();
        OLED_DisplayText("API ERROR");
        return;
    }

    OLED_Clear();
    OLED_DisplayText("Info OK\nPress ENTER");

    // Confirm before writing
    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        if (String(input) == "`") break;
    }

    // Prepare to write tag
    String tagWriteData = serverToTagPlaintext(infoResponse);
    Serial.println("[MODE2] Data to be written:");
    Serial.println(tagWriteData);

    OLED_Clear();
    OLED_DisplayText("Approach tag\nPress ENTER");

    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        if (String(input) == "`") break;
    }

    rfid_writeTag(tagWriteData);
    Serial.println("✅ Tag written successfully.");

    OLED_Clear();
    OLED_DisplayText("1-Service\n2-More");

    // Final decision
    while (true) {
        char* input = KEYBOARD_READ(0);
        globalServiceMonitor(input);
        String choice = String(input);

        if (choice == "1") {
            Serial.println("[MODE2] Switching to SERVICE MODE...");
            enterServiceMode();
            return;
        } else if (choice == "2") {
            Serial.println("[MODE2] Restarting mode2...");
            goto read_tag_again;  // Restart tag registration
        }
    }


    Serial.println("ex[MODE2]");
}


void mode3() {
  WiFi.mode(WIFI_STA);
  scanAndStoreNetworks();

  if (storedCount == 0) {
    OLED_DisplayText("No networks", 2);
    delay(2000);
    enterServiceMode();
    return;
  }

  uint8_t selectedIndex = 0;

  while (true) {
    // Show current selection
    String ssid = getSSIDFromStoredBSSID(storedNetworks[selectedIndex].bssid);
    String rssi = String(storedNetworks[selectedIndex].rssi) + " dBm";

    OLED_DisplayStrictText(ssid, rssi);

    // Wait for control key
    char key = KEYBOARD_READ_CONTROL();

    switch (key) {
      case 'U':
        if (selectedIndex > 0) selectedIndex--;
        OLED_Clear();
        OLED_DisplayText("Loading...", 2);
        break;

      case 'D':
        if (selectedIndex < storedCount - 1) selectedIndex++;
        OLED_Clear();
        OLED_DisplayText("Loading...", 2);
        break;

      case '3':
        OLED_Clear();
        OLED_DisplayText("Rescanning...", 1);
        scanAndStoreNetworks();
        selectedIndex = 0;
        break;

      case '1':
        enterServiceMode();
        return;

      case 'E': {
        String selectedSSID = getSSIDFromStoredBSSID(storedNetworks[selectedIndex].bssid);

        OLED_DisplayStrictText(selectedSSID, "ENTER PASS");
        delay(1000);
        OLED_Clear();
        OLED_DisplayText("Type pass:", 2);

        char* pass = KEYBOARD_READ(1);

        if (pass == nullptr || pass[0] == '@' || pass[0] == '\0') {
          OLED_DisplayStrictText("Canceled", "");
          delay(1500);
          enterServiceMode();
          return;
        }

        OLED_DisplayStrictText(selectedSSID, String(pass));
        delay(2000);

        // Try to connect to Wi-Fi
        OLED_DisplayText("Connecting...", 2);
        WiFi.begin(selectedSSID.c_str(), pass);

        unsigned long start = millis();
        bool connected = false;
        while (millis() - start < 10000) {  // Wait up to 10 sec
          if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
          }
          delay(300);
        }

        if (!connected) {
          OLED_DisplayStrictText("Connect failed", "");
          delay(3000);
          enterServiceMode();
          return;
        }

        String ip = WiFi.localIP().toString();
        OLED_DisplayStrictText(ip, "Auth...");

        // Try authentication
        bool authed = authenticate();

        if (authed) {
          OLED_DisplayStrictText(ip, "SERVER ACTIVE");
        } else {
          OLED_DisplayStrictText(ip, "SERVER OFFLINE");

          // Allow retry via '9'
          while (true) {
            char k = KEYBOARD_READ_CONTROL();
            if (k == '9') {
              OLED_DisplayStrictText("Retrying...", "");
              bool retrySuccess = authenticate();
              if (retrySuccess) {
                OLED_DisplayStrictText(ip, "SERVER ACTIVE");
                break;
              } else {
                OLED_DisplayStrictText(ip, "AUTH FAILED");
              }
            } else if (k == '1') {
              enterServiceMode();
              return;
            }
          }
        }

        delay(3000);
        enterServiceMode();
        return;
      }

      default:
        break;
    }
  }
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

      LogStuff();
    // -----------------------------
    // TIMER pentru baterie (5 min)
    // -----------------------------
    static unsigned long lastBatteryUpdate = 0;
    const unsigned long BATTERY_INTERVAL = 300000; // 5 min

    // -----------------------------
    // TIMER pentru ceas OLED
    // -----------------------------
    static int lastMinute = -1;

    unsigned long nowMillis = millis();

    // -----------------------------
    // RULEAZĂ CONTINUU
    // -----------------------------
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    // -----------------------------
    // UPDATE BATERIE la 5 minute
    // -----------------------------
    if (nowMillis - lastBatteryUpdate >= BATTERY_INTERVAL) {
        lastBatteryUpdate = nowMillis;

        float result = GPIO_CHECK_STATUS_AND_BATTERY();
        int tamper = (result < 150.0f) ? 1 : 0;
        float battery = (tamper == 1) ? (result - 100.0f) : (result - 200.0f);

        Serial.print("Tamper Status: ");
        Serial.print(tamper);
        Serial.print(" | Battery Voltage: ");
        Serial.print(battery, 1);
        Serial.println("V");

        UPDATE_BATTERY_SENSOR(battery);
    }

    // -----------------------------
    // AFIȘARE ORĂ PE OLED
    // -----------------------------
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {

        // Dacă s-a schimbat minutul
        if (timeinfo.tm_min != lastMinute) {
            lastMinute = timeinfo.tm_min;

            char timeStr[6]; // HH:MM + NULL
            snprintf(timeStr, sizeof(timeStr),
                     "%02d:%02d",
                     timeinfo.tm_hour,
                     timeinfo.tm_min);

            OLED_Clear();
            OLED_DisplayText(timeStr);

            Serial.println("🕒 OLED Time Updated: " + String(timeStr));
        }
    }
}


// MODE 6
void mode6()
{
    char* input = KEYBOARD_READ(0);
    globalServiceMonitor(input);

    OLED_DisplayText("MODE 6 ACTIVE");
    connectWiFi("Minerii din bomboclat","castravete");
    enterServiceMode();
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
