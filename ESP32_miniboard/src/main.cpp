#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "network.h"
#include "gpio.h"
#include "keyboard.h"
#include "oled.h"
#include "rfid.h"
#include "main.h"


extern "C" {
  #include "log.h"
}



#define MAX_INPUT_LEN 16


#define MAX_NETWORKS 10


uint8_t wifiSortedIndices[MAX_NETWORKS];
uint8_t wifiNetworkCount = 0;
int8_t wifiSelectedIndex = 0;


// ════════════════════════════════════════════════════════════════
// 📌 GLOBAL STATE & CONFIGURATION
// ════════════════════════════════════════════════════════════════


int currentMode = DEFAULT_START_MODE;

char currentInput[MAX_INPUT_LEN + 1]; // +1 for null terminator
int inputLength = 0;


// ───── Service Mode State ─────
String serviceSteps[3] = { "123", "321", "111" };
int serviceStepIndex = 0;
int serviceEnterCount = 0;
bool serviceActive = false;
unsigned long serviceStartTime = 0;

// ════════════════════════════════════════════════════════════════
// 🔧 UTILITY FUNCTIONS
// ════════════════════════════════════════════════════════════════



// ════════════════════════════════════════════════════════════════
// 📝 LOGGING FUNCTION
// ════════════════════════════════════════════════════════════════




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



void SERVICE_MENU()
{
    OLED_Clear();
    OLED_DisplayText("SERVICE MENU", 2);

    delay(500);

    unsigned long start = millis();
    const unsigned long TIMEOUT = 10000;

    while (millis() - start < TIMEOUT)
    {
        if (!KEYBOARD_ACTIVE()) {
            delay(20);
            continue;
        }

        char* input = KEYBOARD_READ(0);

        if (strcmp(input, "@TIME") == 0 || strcmp(input, "@NONE") == 0)
            return;

        if (strlen(input) == 0)
            return;

        LOG_MIN("SERVICE INPUT RECEIVED:");
        LOG(input);

        // -------------------------------
        // must start with '5'
        // -------------------------------
        if (input[0] != '5')
        {
            OLED_Clear();
            OLED_DisplayText("MENU INVALID", 2);
            OLED_Update();
            delay(800);
            return;
        }

        // convert to int
        int menuCode = atoi(input);

        // reset timeout once valid family detected
        start = millis();

        // -------------------------------
        // SWITCH SERVICE MODES
        // -------------------------------
        switch (menuCode)
        {
            case 55555:      // 🔐 password-protected example
            {
                LOG_MIN("55555 selected -> password request");

                OLED_Clear();
                OLED_DisplayText("PIN:", 2);
                OLED_Update();

                while (millis() - start < TIMEOUT)
                {
                    char* pin = KEYBOARD_READ(0);

                    if (strcmp(pin, "@TIME") == 0 || strcmp(pin, "@NONE") == 0)
                        return;

                    if (strcmp(pin, "12345") == 0)
                    {
                        OLED_Clear();
                        OLED_DisplayText("SUCCESS!", 2);
                        enterServiceMode();

                        LOG_MIN("PASSWORD OK");
                        delay(800);

                        return;
                    }
                    else
                    {
                        OLED_Clear();
                        OLED_DisplayText("WRONG PIN", 2);
                        OLED_Update();
                        LOG_MIN("PASSWORD FAIL");
                        delay(800);
                        return;
                    }
                }

                LOG_MIN("TIMEOUT WAITING PASSWORD");
                return;
            }

            // ---------- add your future menus here ----------
            case 50001:
                LOG_MIN("option 50001 selected");
                OLED_Clear();
                OLED_DisplayText("OP 50001", 2);
                OLED_Update();
                delay(800);
                return;

            case 50002:
                LOG_MIN("option 50002 selected");
                OLED_Clear();
                OLED_DisplayText("OP 50002", 2);
                OLED_Update();
                delay(800);
                return;

            // -----------------------------------------------
            // DEFAULT — unknown 5xxxx menu
            // -----------------------------------------------
            default:
                OLED_Clear();
                OLED_DisplayText("MENU INVALID", 2);
                OLED_Update();
                LOG_MIN("INVALID MENU");
                delay(800);
                return;
        }
    }

    LOG_MIN("SERVICE MENU TIMEOUT");
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


void serial_logger(const char *msg)
{
    Serial.println(msg);
}


void setup() {
    Serial.begin(115200);
    delay(50);

    //connectWiFi("C004","53638335");
    connectWiFi("Minerii din bomboclat","castravete");
    GPIO_INIT();
    OLED_Init();
     OLED_QueueInit();

     OLED_DisplayText("OLED OK");
    KEYBOARD_INIT();
    OLED_DisplayText("KEY OK");
    rfid_init();
    OLED_DisplayText("RFID OK");


    syncTime();

    // register backend
    log_set_output(serial_logger);

    // set global level here (affects entire program)
    log_set_level(LOG_FULL);   // or LOG_FULL / LOG_OFF


    logSensorEvent(6, "system", "ESP32 boot completed", 2);

    initServiceSequence();

    //OLED_DisplayText("READY");
  OLED_RequestDisplayText("Hello", 1, 50, 3);




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
    OLED_DisplayText(String(mode));
}

void runCurrentMode()
{
    //monitorServerStatus(currentMode);

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


    unsigned long lastCallTime = 0; // Timpul ultimei apelări
const unsigned long interval = 500; // 500ms

// MODE 0 — SERVICE MODE



void mode_service()
{




unsigned long currentMillis = millis();

  // Verificăm dacă au trecut mai mult de 500ms
  if (currentMillis - lastCallTime >= interval) {
    LogStuff();                // apelăm funcția
    lastCallTime = currentMillis; // actualizăm timpul ultimei apelări
  }




    // always run these (fast + lightweight)

    // LogStuff();   // if you use i
    // --------------------------------------
    // KEYBOARD PART — only sometimes runs
    // --------------------------------------
    if (KEYBOARD_ACTIVE()) {

        // do the full blocking read only when actually active
        char* input = KEYBOARD_READ(0);



        // skip processing on special return tokens
        if (strcmp(input, "@TIME") == 0 || strcmp(input, "@NONE") == 0) {
            // don't return from the function — just skip rest
        }

        // ---- NEW: service secret code ----
        else if (strcmp(input, "@service") == 0) {

            OLED_Clear();
            OLED_DisplayText("insert    OP[201]", 2);
            OLED_Update();
            delay(800);
            SERVICE_MENU();

            // stay in service mode, no return
        }
        else if (strlen(input) > 0)
        {
            int target = atoi(input);

            if (target > 0 && target < MAX_MODES)
            {
                OLED_Clear();
                OLED_DisplayText("Loading MODE " + String(target));
                delay(400);
                setMode(target);
                // (here a return is OK because you're changing mode)
            }
            else
            {
                OLED_DisplayText("Invalid selection");
            }
        }
    }

    // --------------------------------------
    // everything else in service mode keeps running
    // --------------------------------------
    // … background things here (battery, comms, etc.)
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

    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }



        if (strcmp(input, "@") == 0 || strlen(input) == 0) goto start_over;



        // ─── Normal PIN entry ───
        uint32_t pin = strtoul(input, NULL, 10);
        if (pin == 999999) {
            pin_correct = true;
        }
        break;

    }//end of keyboard active
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

        if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }



        if (strcmp(input, "@") != 0 && strcmp(input, "`") != 0)
            goto  start_over;

        delay(100);

    }//end of keyboard active
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


    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }


        if (String(input) == "`") break;
    }
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

        if (KEYBOARD_ACTIVE()) {
        char* input = KEYBOARD_READ(0);
        if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }


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
    }

    Serial.println("[MODE2] Tag detected:");
    Serial.println(uid);
    OLED_Clear();
    OLED_DisplayText("Tag OK\nPress ENTER");

    // Confirm before registering tag
    while (true) {

    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }



        if (String(input) == "`") break;
    }
    }

    registerTAG(uid, "Test tag added via ESP32");
    Serial.println("[MODE2] Tag registered");

    OLED_Clear();
    OLED_DisplayText("Registered\nPress ENTER");

    // Confirm before next step
    while (true) {
           if (KEYBOARD_ACTIVE()) {
        char* input = KEYBOARD_READ(0);
        if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }




        if (String(input) == "`") break;
    }
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
        if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }



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
          if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }

        if (String(input) == "`") break;
    }

    rfid_writeTag(tagWriteData);
    Serial.println("✅ Tag written successfully.");

    OLED_Clear();
    OLED_DisplayText("1-Service\n2-More");

    // Final decision
    while (true) {
        char* input = KEYBOARD_READ(0);
          if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
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

    if (KEYBOARD_ACTIVE()) {
    char* pass = KEYBOARD_READ(0);
    if (strcmp(pass, "@service") == 0) { SERVICE_MENU();  }


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


} //end of lkeyboard active
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
    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }

    OLED_DisplayText("MODE 4 ACTIVE");
}


// MODE 5
void mode5() {
    LogStuff();

    // -----------------------------
    // TIMER pentru baterie (5 min)
    // -----------------------------
    static unsigned long lastBatteryUpdate = 0;
    //const unsigned long BATTERY_INTERVAL = 300000; // 5 min
    const unsigned long BATTERY_INTERVAL = 30000; // 30s min


    // -----------------------------
    // TIMER pentru ceas OLED
    // -----------------------------
    static int lastMinute = -1;
    unsigned long nowMillis = millis();

    // -----------------------------
    // RULEAZĂ CONTINUU
    // -----------------------------
    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }

    // -----------------------------
    // UPDATE BATERIE la 5 minute
    // -----------------------------
    if (nowMillis - lastBatteryUpdate >= BATTERY_INTERVAL) {
        lastBatteryUpdate = nowMillis;

        float battery = 0.0f;
        uint8_t tamper = 0;
         GPIO_CHECK_BATTERY(&battery, &tamper);  // pointer-style function

        Serial.print("Tamper Status: ");
        Serial.print(tamper);
        Serial.print(" | Battery Voltage: ");
        Serial.print(battery, 1);
        Serial.println("V");

        UPDATE_BATTERY_SENSOR(battery);

        // Display on OLED
        OLED_Clear();
        char oledStr[32];
        snprintf(oledStr, sizeof(oledStr),
                 "Batt: %.1fV\nTamper: %d", battery, tamper);
        OLED_DisplayText(oledStr);

            int adcVal = GPIO_READ(105); // 0-4095
            Serial.println(adcVal);
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
    OLED_DisplayText("MODE 6 ACTIVE");
    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }


    connectWiFi("Minerii din bomboclat","castravete");
    enterServiceMode();
}


// MODE 7
void mode7()
{
    OLED_DisplayText("MODE 7 ACTIVE");
    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }


}


// MODE 8
void mode8()
{
    LOG("inside working hard");
    OLED_DisplayText("MODE 8 ACTIVE");
delay(150);

    if (KEYBOARD_ACTIVE()) {

        LOG("enter keyboard");
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }


}


// MODE 9
void mode9()
{

 OLED_DisplayText("MODE 9 ACTIVE");
    if (KEYBOARD_ACTIVE()) {
    char* input = KEYBOARD_READ(0);
    if (strcmp(input, "@service") == 0) { SERVICE_MENU();  }
    }


}



// =====================================================================
// MAIN LOOP
// =====================================================================



void loop()
{
    runCurrentMode();


}



