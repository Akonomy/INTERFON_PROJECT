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

        if (strcmp(input, "@") == 0 )
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

                    if (strcmp(pin, "@") == 0  )
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
            {
                LOG_MIN("option 50001 selected");

                OLED_Clear();
                OLED_DisplayText("CHECK TAG", 2);
                OLED_Update();

                // încercăm să verificăm tag-ul de service
                uint8_t ok = check_service_tag();

                if (ok == 1) {
                    LOG_MIN("SERVICE TAG OK");
                    OLED_Clear();
                    OLED_DisplayText("SERVICE OK", 2);
                    OLED_Update();
                    delay(200);

                    enterServiceMode();
                    return;
                }

                // tag invalid sau nu poate fi citit
                LOG("SERVICE TAG INVALID or READ FAIL");

                OLED_Clear();
                OLED_DisplayText("TAG ERROR", 2);
                OLED_Update();
                delay(200);

                return;
            }


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
    logSensorEvent( 6,    "system", "SERVICE MODE ACCESED", 3 );
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
    monitorServerStatus(currentMode);

    switch (currentMode)
    {
        case 0: mode_service(); break;
        case 1: mode1(); break;  //mode public
        case 2: mode2(); break;  //mode  register tag
        case 3: mode3(); break;  //mode connect to wifi
        case 4: mode4(); break; //mode register service tag
        case 115: mode5(); break; // BETA mode log a msg
        case 116: mode6(); break; //none
        case 7: mode7(); break; //mock tag register register
        case 118: mode8(); break;//none
        case 9: mode9(); break;//BETA test keyboard
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


void mode_service() //default mode start
{




    unsigned long currentMillis = millis();

    // periodic logging
    if (currentMillis - lastCallTime >= interval) {
        LogStuff();
        lastCallTime = currentMillis;
    }


        // read up to 2 digits from keypad
     OLED_DisplayLine("press any:");
        OLED_DisplayTime();
        uint32_t inputValue = KEYBOARD_READ_NUMBER(2,5);

        // compute error threshold (10^2 = 100)
        uint32_t errorValue = 100u;

        // ---- ERROR (timeout or invalid) ----
        if (inputValue >= errorValue)
        {
            OLED_DisplayLine("time :");


            delay(1000);
            return;   // stay in service mode
        }

        // ---- VALID NUMBER ----
        if (inputValue > 0u && inputValue < (uint32_t)MAX_MODES)
        {
            OLED_Clear();
            OLED_DisplayText("Loading MODE ", 2);
            OLED_DisplayNumber(inputValue);
            OLED_Update();
            delay(400);

            setMode((uint8_t)inputValue);
            return;
        }
        else
        {
            OLED_Clear();
            OLED_DisplayText("Invalid mode", 2);
            OLED_Update();
            delay(600);
            OLED_Clear();
        }


    // background tasks run here automatically
}

 static unsigned long lastMode1EnterMillis = 0;
bool canEnterMode1(uint32_t pin)
{
    const unsigned long COOLDOWN = 120000UL; // 2 minutes

    // special override PIN
    if (pin == 413207)
        return true;

    // normal cooldown rule
    unsigned long now = millis();
    if (now - lastMode1EnterMillis >= COOLDOWN)
        return true;

    return false;
}


void mode1()
{
    String uid, data,log_result,log_details;


    bool have_pin = false;
    bool pin_ok = false;
    bool have_tag = false;
    bool tag_ok = false;

    uint32_t pin;


    /************************************************************
     * STEP 0 — OPTIONAL KEYBOARD
     ************************************************************/

step0_readKeyboard:

LOG("STEP0");
have_pin = false;
pin_ok = false;
have_tag = false;
tag_ok = false;


delay(1);
OLED_DisplayLine("PRESS AND HOLD",1);

unsigned long t0 = millis();

while (millis() - t0 < 10000UL)
{
        LogStuff();

         // valoare imposibilă → forțează primul update


         OLED_DisplayLine(":: ENTER PIN ::");

            OLED_DisplayTime();


        // ***** READ UP TO 6 DIGITS *****
        pin = KEYBOARD_READ_NUMBER(6,5);
        LOG("PIN");
          Serial.println(pin);

        // ***** ERROR / TIMEOUT / CANCEL *****
        // ERROR_VALUE = 10^6 = 1,000,000
        if (pin >= 1000000UL)
        {
            LOG("PIN TIMEOUT OR INVALID → GO SERVICE");
            have_pin = false;
            pin_ok = false;
            goto step1_service;
        }

        // ***** VALID NUMERIC PIN ENTERED *****
        have_pin = true;


       if (pin== 413207)
        {
            LOG("GO STEP SERVICE");
            goto step1_service;
        }



        // backdoor PIN
        if (pin == 1234)
        {

            pin_ok = true;
            LOG("GO STEP2");

            goto step2_readTag;
        }



        // normal PIN: require TAG after PIN
        LOG("GO STEP2 READ");
        goto step2_readTag;



}

// ---------- TIMEOUT WHOLE STEP ----------
have_pin = false;
pin_ok = false;
LOG("STEP0 TIMEOUT → SERVICE");
goto step1_service;







    /************************************************************
     * STEP 1 — SERVICE TAG BACKDOOR
     ************************************************************/
step1_service:
LOG("SERVICE CHECK");
     OLED_DisplayText("SERVICE CHECK  (:", 3);


     if (!canEnterMode1(pin))
{
    goto step4_stuff;
}


    if (check_service_tag() == 1)
    {
        OLED_Clear();
        OLED_DisplayText("(:", 2);
        delay(500);
        enterServiceMode();
        return;
    }
     OLED_DisplayText("(:  : :)", 2);

     LOG("GO STEP4");

     lastMode1EnterMillis = millis();

     if(have_pin){goto step2_readTag; }


    goto step4_stuff;

    /************************************************************
     * STEP 2 — READ USER TAG (ONLY when normal pin used)
     ************************************************************/
step2_readTag:
    LOG("STEP 2 CHECK TAG");

    if (have_pin)
    {
        const unsigned long timeout = 20000UL;
        const unsigned long retry   = 2700UL;

        unsigned long st = millis();

        OLED_Clear();
        OLED_DisplayText("SCAN TAG ");

        while (millis() - st < timeout)
        {


            OLED_DisplayText("SCAN TAG");
            delay(100);
            if (rfid_readTag(uid, data))
            {
                have_tag = true;

                ParsedTagData parsed = parsePlaintext(data);

                if (parsed.valid && checkTag(uid, parsed.encrypted_info) == 1)
                    tag_ok = true;

                break;
            }
            LogStuff();

            OLED_DisplayText("SCAN TAG>");
            delay(retry);


        }
    }
    OLED_DisplayText("CHECKING INFO" , 2);
    LOG("GO STEP 3 VALIDARE");
    goto step3_validate;


    /************************************************************
     * STEP 3 — FINAL VALIDATION (SWAPPED)
     ************************************************************/
step3_validate:

LOG("STEP3 VALIDARE");



// fără PIN și fără TAG
if (!have_pin && !have_tag)
{
    OLED_DisplayText("NO PIN", 2);
    delay(800);
    goto step4_stuff;
}

if (have_pin && !have_tag)
{
    OLED_DisplayText("NO CARD", 2);

    log_result  = "ATTEMPT";
    log_details = "pin entered, no card";

    LOG_ACCESS("NONE", log_result, log_details);

    delay(1000);
    goto step4_stuff;
}


if (have_pin && have_tag)
{
      logSensorEvent( 6,    "public", "<<DOOR ACTIVITY>>", 3 );
    if (pin_ok && tag_ok)
    {
        OLED_DisplayText("ACCESS OK", 2);
        log_result  = "ALLOWED";
         logSensorEvent( 6,    "public", "<<ALLOWED>>", 3 );
        log_details = "pin and card valid";
    }
    else if (!pin_ok && !tag_ok)
    {
        OLED_DisplayText("REJECTED", 2);
        log_result  = "PIN_AND_CARD_INVALID";
        log_details = "pin invalid, card invalid";

    }
    else if (!pin_ok)
    {
        OLED_DisplayText("REJECTED", 2);
        log_result  = "PIN_INVALID";
        log_details = "pin invalid";
    }
    else
    {
        OLED_DisplayText("REJECTED", 2);
        log_result  = "CARD_INVALID";
        log_details = "card invalid";
    }

    LOG_ACCESS(uid,  log_result, log_details);

    delay(1200);
    goto step4_stuff;
}






    /************************************************************
     * STEP 4 — ALWAYS DO-STUFF BLOCK
     ************************************************************/
step4_stuff:


    {

        LOG("STEP4");
        unsigned long t = millis();

           // updateClockDisplay();
             OLED_DisplayText("(: ", 3);
             delay(800);
            LogStuff();

            // service escape is still allowed here
            monitorServerStatus(1);
        }
        OLED_DisplayText("(:)" , 3);
        delay(500);


    // loop mode1 forever like kiosk


    goto step0_readKeyboard;

}




//END END END







void mode2()
{
    logSensorEvent( 6,    "register", "<<INREGISTRARE TAG>>", 3 );
    LOG_MIN("MODE2 start");

    String uid;
    String data;
    String infoResponse;

    /************************************************************
     * WAIT FOR USER TO START
     ************************************************************/
    OLED_Clear();
    OLED_DisplayStrictText(
        "APASA ENTER PENTRU",
        "A INREGISTRA TAG"
    );

    while (true)
    {
        char key = KEYBOARD_READ_CONTROL();
        if (key == '1' || key == 'B') { enterServiceMode(); return; }
        if (key == 'E') break;
    }

    /************************************************************
     * ─────────────── STEP 1 — READ TAG ───────────────
     ************************************************************/
step1_readTag:

    LOG("STEP1: read tag");

    {
        const unsigned long timeoutMs = 60000UL; // 1 min
        const unsigned long intervalMs = 3000UL; // 3 s

        unsigned long start = millis();

        while (millis() - start < timeoutMs)
        {
            OLED_Clear();
            OLED_DisplayStrictText("APROPIATI TAGUL", "DE CITITOR");

            if (rfid_readTag(uid, data))
            {
                LOG_MIN("TAG DETECTED");
                goto step2_registerTag;
            }

            OLED_DisplayStrictText("TAG NEDISPONIBIL", "RETRY AUTO...");
            delay(intervalMs);
        }
    }

    // ---- failure handling for step 1 ----
    {
        LOG_MIN("STEP1 timeout");

        unsigned long start = millis();
        const unsigned long wait30 = 30000UL;

        while (millis() - start < wait30)
        {
            unsigned long secLeft = (wait30 - (millis() - start)) / 1000;

            char line2[21];
            snprintf(line2, sizeof(line2), "%lu sec pana service", secLeft);

            OLED_Clear();
            OLED_DisplayStrictText("APASA TASTA", line2);

            if (KEYBOARD_ACTIVE())
            {
                LOG_MIN("Retry STEP1");
                goto step1_readTag;
            }

            delay(200);
        }

        enterServiceMode();
        return;
    }

    /************************************************************
     * ─────────────── STEP 2 — REGISTER TAG ───────────────
     ************************************************************/
step2_registerTag:

    LOG_MIN("STEP2: register TAG");

    registerTAG(uid, "Test tag added via ESP32");

    OLED_Clear();
    OLED_DisplayStrictText("INREGISTRARE...", "ASTEAPTATI");

    {
        uint8_t approved = waitForTagFullyApproved(uid);

        if (approved == 1)
        {
            infoResponse = getTagInfo(uid);
            if (infoResponse == "") {
                LOG_MIN("Server info empty");
                goto step2_registerTag_fail;
            }

            goto step3_writeTag;
        }
    }

step2_registerTag_fail:

    LOG_MIN("STEP2 failed");

    {
        unsigned long start = millis();
        const unsigned long wait30 = 30000UL;

        while (millis() - start < wait30)
        {
            unsigned long secLeft = (wait30 - (millis() - start)) / 1000;

            char line2[21];
            snprintf(line2, sizeof(line2), "%lu sec pana service", secLeft);

            OLED_Clear();
            OLED_DisplayStrictText("INREGISTRARE ESUATA", line2);

            if (KEYBOARD_ACTIVE())
            {
                LOG_MIN("Retry STEP2");
                goto step2_registerTag;
            }

            delay(200);
        }

        enterServiceMode();
        return;
    }

    /************************************************************
     * ─────────────── STEP 3 — WRITE TAG ───────────────
     ************************************************************/
step3_writeTag:

    LOG_MIN("STEP3: write TAG");

    String tagWriteData = serverToTagPlaintext(infoResponse);

    {
        const unsigned long timeoutMs = 60000UL;
        const unsigned long intervalMs = 3500UL;

        unsigned long start = millis();

        while (millis() - start < timeoutMs)
        {
            OLED_Clear();
            OLED_DisplayStrictText("APROPIATI TAGUL", "PENTRU SCRIERE");

            if (rfid_writeTag(tagWriteData))
            {
                LOG_MIN("WRITE OK");

                OLED_Clear();
                OLED_DisplayStrictText("SCRIERE OK", "TAG FINALIZAT");
                logSensorEvent( 6,    "register", "<<INREGISTRARE TAG CU SUCCES>>", 3 );
                delay(1200);

                enterServiceMode();
                return;
            }

            OLED_DisplayStrictText("SCRIERE ESUATA", "RETRY AUTO...");
            logSensorEvent( 6,    "register", "<<FAIL REGISTER TAG>>", 3 );
            delay(intervalMs);
        }
    }

    // ---- failure handling for step 3 ----
    {
        LOG_MIN("STEP3 timeout");

        unsigned long start = millis();
        const unsigned long wait30 = 30000UL;

        while (millis() - start < wait30)
        {
            unsigned long secLeft = (wait30 - (millis() - start)) / 1000;

            char line2[21];
            snprintf(line2, sizeof(line2), "%lu sec pana service", secLeft);

            OLED_Clear();
            OLED_DisplayStrictText("SCRIERE ESUATA", line2);

            if (KEYBOARD_ACTIVE())
            {
                LOG_MIN("Retry STEP3");
                goto step3_writeTag;
            }

            delay(200);
        }

        enterServiceMode();
        return;
    }
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
     OLED_Clear();

    char* pass = KEYBOARD_READ_STRING();   // blocking keyboard entry

    // ---------- canceled ----------
    if (pass == nullptr || pass[0] == '\0') {
        OLED_DisplayStrictText("Canceled", "");
        delay(1500);
        enterServiceMode();
        return;
    }

    // ---------- show SSID + password ----------
    OLED_DisplayStrictText(selectedSSID, String(pass));
    delay(2000);

    OLED_DisplayText("Connecting...", 2);
    Serial.println(pass);
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
          logSensorEvent( 6,    "wifi_connect", "<<SERVER ACTIV>>", 3 );
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
    static unsigned long lastSkipLog = 0;

    char key = 0;

    if (KEYBOARD_ACTIVE()) {
        key = KEYBOARD_READ_CONTROL();
    }

    // ==========================
    // dacă NU e ENTER -> ieșim
    // ==========================
    if (key != 'E') {

        unsigned long now = millis();

        if (now - lastSkipLog > 10000) {
            lastSkipLog = now;
            LOG("MODE4 idle / waiting ENTER");
        }

        return;
    }

    // ==========================
    // ENTER apăsat -> execută flow-ul complet
    // ==========================
    LOG_MIN("MODE4 ENTER pressed");

    OLED_Clear();
    OLED_DisplayText("Checking TAG...");

    String uid, data;

    // încercăm să citim TAG
    if (!rfid_readTag(uid, data)) {

        OLED_Clear();
        OLED_DisplayText("TAG ERROR");
        LOG_MIN("TAG read failed / no tag present");

        delay(200);

        // NU scriem, NU verificăm
        LOG("Skipping write – invalid or missing tag");

        return;
    }

    LOG_MIN("TAG detected OK");

    // ==========================
    // scriere SERVICE TAG
    // ==========================
    uint8_t reg = register_service_tag();

    if (!reg) {
        LOG_MIN("SERVICE TAG WRITE FAILED");

        OLED_Clear();
        OLED_DisplayText("WRITE ERROR");
        delay(200);

        return;
    }

    // ==========================
    // verificare SERVICE TAG
    // ==========================
    uint8_t chk = check_service_tag();

    if (!chk) {
        LOG_MIN("SERVICE TAG VERIFY FAILED");

        OLED_Clear();
        OLED_DisplayText("VERIFY ERROR");
        delay(200);

        return;
    }

    // ==========================
    // SUCCESS
    // ==========================
    LOG_MIN("SERVICE TAG OK");

    OLED_Clear();
    OLED_DisplayText("SERVICE OK");

    delay(200);

    enterServiceMode();
}




// MODE 5
void mode5() {
    LogStuff();

    // -----------------------------
    // TIMER pentru baterie (5 min)
    // -----------------------------
    static unsigned long lastBatteryUpdate = 0;
    const unsigned long BATTERY_INTERVAL = 300000; // 5 min
    //const unsigned long BATTERY_INTERVAL = 30000; // 30s min


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
    logSensorEvent( 6,    "test", "<<TEST REGISTER TAG>>", 3 );
    OLED_Clear();
    OLED_DisplayText("MODE 7 ACTIVE", 2);

    String uid = "TEST12345";

    // 1) send mock registration request
    registerTAG(uid, "Mock test tag");

    OLED_Clear();
    OLED_DisplayStrictText("REQUEST SENT", "WAITING STATUS");
    delay(800);

    // 2) wait/poll for approval workflow
    uint8_t result = waitForTagFullyApproved(uid);

    // 3) show final result
    OLED_Clear();

    if (result == 1)
    {
        OLED_DisplayStrictText(
            "FINAL RESULT:",
            "TAG FULL APPROVED"
        );
    }
    else
    {
        OLED_DisplayStrictText(
            "FINAL RESULT:",
            "NOT APPROVED"
        );
    }

    delay(1500);

    // 4) always go back to service mode
    enterServiceMode();
}



// MODE 8
void mode8()
{
     TESTkeypadScan();

    enterServiceMode();

}


// MODE 9
void mode9()
{
logSensorEvent( 6,    "test", "<<TEST KEYBOARD>>", 3 );
 OLED_DisplayText("TEST ACTIVE ");
 delay(1000);
 //OLED_TestSimulation();

 delay(1000);

 //keypadTestScanner();

  keypadMultiTapTest();

LOG("RESULT::::::");



 enterServiceMode();


}



// =====================================================================
// MAIN LOOP
// =====================================================================



void loop()
{
    runCurrentMode();


}



