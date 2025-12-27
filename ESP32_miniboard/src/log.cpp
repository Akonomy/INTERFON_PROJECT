#include "log.h"
#include <stdio.h>
#include <string.h>

static volatile LogLevel global_log_level = LOG_FULL;
static log_output_fn output_function = 0;

void log_set_level(LogLevel level) {
    global_log_level = level;
}

LogLevel log_get_level(void) {
    return global_log_level;
}

void log_set_output(log_output_fn fn) {
    output_function = fn;
}

void log_min_tag(const char *func, const char *msg)
{
    if (!output_function) return;
    if (global_log_level < LOG_MINIMAL) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "[%s] %s", func, msg);
    output_function(buf);
}

void log_full_tag(const char *func, const char *msg)
{
    if (!output_function) return;
    if (global_log_level < LOG_FULL) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "[%s] %s", func, msg);
    output_function(buf);
}







bool canLog = true;

// ===== Throttle State =====
uint8_t alertCount = 0;
uint8_t lastSeverity = 255;
String lastReason = "";
unsigned long lastAlertTime = 0;
const unsigned long ALERT_COOLDOWN_MS = 5UL * 60UL * 1000UL; // 5 minutes
const uint8_t ALERT_BURST_LIMIT = 1;

void LogStuff() {
    const uint16_t LOG_INTERVAL_MIN = 3;

    float battery = 0.0f;
    uint8_t tamper = 0;
    GPIO_CHECK_BATTERY(&battery, &tamper);

    uint8_t severity = 6; // INFO
    bool alertTriggered = false;

    // Wi-Fi
    int rssi = -127;
    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected) rssi = WiFi.RSSI();

    // ==== motivul alertei (nume categorie) ====
    const char* triggerSource = "system";
    const char* triggerReason = "log";

    // -------- TAMPER --------
    if (tamper) {
        severity = 0;  // EMERG
        alertTriggered = true;
        triggerSource  = "tamper";
        triggerReason  = "TAMPER";
    }

    // -------- BATERIE --------
    if (!alertTriggered) {
        if (battery < 3.3f) {
            severity = 0;
            alertTriggered = true;
            triggerSource  = "battery";
            triggerReason  = "BATERIE CRITICA";
        } else if (battery < 4.3f) {
            severity = 1;
            alertTriggered = true;
            triggerSource  = "battery";
            triggerReason  = "ALIMENTARE PE BATERIE INTERNA";
        } else if (battery < 6.0f) {
            severity = 4;
            alertTriggered = true;
            triggerSource  = "battery";
            triggerReason  = "BATERIE SCAZUTA";
        }
    }

    // -------- RSSI --------
    if (!alertTriggered && wifiConnected) {
        if (rssi < -85) {
            severity = 2;
            alertTriggered = true;
            triggerSource  = "wifi";
            triggerReason  = "SEMNAL FOARTE SLAB";
        } else if (rssi < -70) {
            severity = 4;
            alertTriggered = true;
            triggerSource  = "wifi";
            triggerReason  = "SEMNAL SLAB";
        }
    }

    // iesire daca doar info & nu e timpul
    if (!alertTriggered && !canLog) return;
    if (!alertTriggered && !shouldLog(LOG_INTERVAL_MIN)) return;

    // ---------- format uptime ----------
    unsigned long uptimeSec = millis() / 1000;
    char uptimeStr[16];
    if (uptimeSec < 120) snprintf(uptimeStr, sizeof(uptimeStr), "%lus", uptimeSec);
    else if (uptimeSec < 7200) snprintf(uptimeStr, sizeof(uptimeStr), "%lum", uptimeSec / 60);
    else snprintf(uptimeStr, sizeof(uptimeStr), "%luh", uptimeSec / 3600);

    // ---------- mesaj ----------
    char msg[256];
    snprintf(msg, sizeof(msg),
        "%s | RSSI=%ddBm | Batt=%.1fV | Tamper=%d | Uptime=%s",
        triggerReason,
        rssi,
        battery,
        tamper,
        uptimeStr
    );

    // !!! AICI nu mai este 'system', ci triggerSource !!!

    // ===================== THROTTLING LOGIC =====================
if (alertTriggered) {

    // same alert type?
    bool sameAlert =
        (severity == lastSeverity) &&
        (lastReason == triggerReason);

    unsigned long now = millis();

    if (sameAlert) {

        // still in burst phase
        if (alertCount >= ALERT_BURST_LIMIT) {
            // only resend after cooldown passes
            if (now - lastAlertTime < ALERT_COOLDOWN_MS) {
                // DROP alert
                return;
            } else {
                // cooldown expired, allow ONE alert and reset burst
                alertCount = 0;
            }
        }

        alertCount++;
    } else {
        // different alert -> reset burst
        alertCount = 1;
    }

    lastSeverity = severity;
    lastReason = triggerReason;
    lastAlertTime = now;
}
else {
    // no alert -> reset throttle
    alertCount = 0;
    lastReason = "";
}



    logSensorEvent(severity, triggerSource, msg, 2);
}
