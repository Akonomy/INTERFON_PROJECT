#include "rfid.h"
#include <Wire.h>
#include <Adafruit_PN532.h>

// =================== CONFIG INTERN ===================
static Adafruit_PN532 *g_nfc = nullptr;

// blocurile folosite pentru date (4..11, 8 blocuri * 16 bytes = 128)
static const uint8_t RFID_FIRST_BLOCK = 4;
static const uint8_t RFID_TOTAL_BLOCKS = 8;

// =================== UTILE INTERNE ===================
static void printHexInternal(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        if (i < len - 1) Serial.print(":");
    }
}

static bool writeBlock(uint8_t block, const uint8_t *data16) {
    if (!g_nfc->mifareclassic_WriteDataBlock(block, (uint8_t *)data16)) {
        Serial.print("❌ Eroare scriere bloc ");
        Serial.println(block);
        return false;
    }
    return true;
}

static bool readBlock(uint8_t block, uint8_t *data16) {
    if (!g_nfc->mifareclassic_ReadDataBlock(block, data16)) {
        Serial.print("❌ Eroare citire bloc ");
        Serial.println(block);
        return false;
    }
    return true;
}

// =================== API PUBLIC ===================

bool rfid_init(uint8_t sdaPin, uint8_t sclPin)
{
    Serial.println("\n=== Init RFID (PN532) ===");

    if (!g_nfc) {
        // I2C software pe pini definiti de utilizator
        g_nfc = new Adafruit_PN532(sdaPin, sclPin);
    }

    Wire.begin(sdaPin, sclPin);
    g_nfc->begin();

    uint32_t ver = g_nfc->getFirmwareVersion();
    if (!ver) {
        Serial.println("❌ PN532 nu raspunde!");
        return false;
    }

    Serial.print("✅ PN532 detectat, versiune 0x");
    Serial.println((ver >> 16) & 0xFFFF, HEX);

    g_nfc->SAMConfig();
    randomSeed(millis());

    Serial.println("RFID initializat. Apropie un card MIFARE Classic 1K...");
    return true;
}

bool rfid_readTag(rfid_tag_t *tag, uint32_t timeoutMs)
{
    if (!g_nfc) return false;

    uint32_t start = millis();
    uint8_t uid[7];
    uint8_t uidLen = 0;

    // asteapta card pana la timeout
    while (true) {
        if (g_nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen)) {
            // Card detectat
            break;
        }
        if (millis() - start > timeoutMs) {
            return false;   // timeout
        }
        delay(50);
    }

    Serial.print("\n💡 Card detectat | UID: ");
    printHexInternal(uid, uidLen);
    Serial.println();

    // copiem UID in struct
    tag->uidLen = uidLen;
    for (uint8_t i = 0; i < uidLen; i++) {
        tag->uid[i] = uid[i];
    }

    // citim datele din blocurile 4..11
    char dataBuf[RFID_MAX_DATA_LEN + 1];
    uint16_t idx = 0;
    for (uint8_t i = 0; i < RFID_TOTAL_BLOCKS && idx < RFID_MAX_DATA_LEN; i++) {
        uint8_t buf[16];
        if (!readBlock(RFID_FIRST_BLOCK + i, buf)) {
            return false;
        }
        for (uint8_t j = 0; j < 16 && idx < RFID_MAX_DATA_LEN; j++) {
            if (buf[j] == 0x00) {
                // consideram 0x00 ca terminator de string
                i = RFID_TOTAL_BLOCKS; // iesim din ambele for-uri
                break;
            }
            dataBuf[idx++] = (char)buf[j];
        }
    }
    dataBuf[idx] = '\0';

    strncpy(tag->data, dataBuf, RFID_MAX_DATA_LEN);
    tag->data[RFID_MAX_DATA_LEN] = '\0';
    tag->dataType = RFID_DATA_TYPE_STRING;  // de fapt nu stim sigur, dar o tratam ca string

    Serial.print("🧾 Data citita: ");
    Serial.println(tag->data);

    return true;
}

bool rfid_writeTag(const char *data,
                   rfid_data_type_t type,
                   const uint8_t *expectedUid,
                   uint8_t expectedUidLen)
{
    if (!g_nfc) return false;

    char localData[RFID_MAX_DATA_LEN + 1];

    // pregatim sirul de scris
    if (type == RFID_DATA_TYPE_RANDOM) {
        rfid_generateRandomString(localData, RFID_MAX_DATA_LEN);
        Serial.println("🧾 Cod random generat pentru scriere:");
        Serial.println(localData);
    } else {
        if (!data) return false;
        strncpy(localData, data, RFID_MAX_DATA_LEN);
        localData[RFID_MAX_DATA_LEN] = '\0';
        Serial.println("🧾 Data primita pentru scriere:");
        Serial.println(localData);
    }

    // asteptam cardul pe care sa scriem (max 5s)
    uint32_t start = millis();
    uint8_t uid[7];
    uint8_t uidLen = 0;

    Serial.println("\n⚙️ Astept un card pentru scriere...");
    while (true) {
        if (g_nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen)) {
            // am gasit un card
            // daca se doreste un UID specific, verificam
            if (expectedUid && expectedUidLen > 0) {
                if (uidLen == expectedUidLen && memcmp(uid, expectedUid, uidLen) == 0) {
                    break; // este cardul dorit
                } else {
                    Serial.println("Card gasit, dar UID-ul nu corespunde. Astept alt card...");
                }
            } else {
                // acceptam orice card
                break;
            }
        }

        if (millis() - start > 5000) {
            Serial.println("❌ Timeout asteptare card pentru scriere.");
            return false;
        }
        delay(50);
    }

    Serial.print("💡 Scriu pe cardul cu UID: ");
    printHexInternal(uid, uidLen);
    Serial.println();

    // scriem codul in blocurile 4..11
    uint8_t dataBlock[16];
    Serial.println("⚙️ Incep scrierea blocurilor...");

    for (uint8_t i = 0; i < RFID_TOTAL_BLOCKS; i++) {
        memset(dataBlock, 0, 16);
        for (uint8_t j = 0; j < 16; j++) {
            uint16_t idx = i * 16 + j;
            if (idx < strlen(localData)) {
                dataBlock[j] = (uint8_t)localData[idx];
            } else {
                dataBlock[j] = 0x00;
            }
        }
        if (!writeBlock(RFID_FIRST_BLOCK + i, dataBlock)) {
            Serial.println("❌ Eroare la scriere! Oprire.");
            return false;
        }
    }

    Serial.println("✅ Scriere completa.");
    return true;
}

void rfid_generateRandomString(char *out, size_t len)
{
    static const char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    size_t nChars = sizeof(chars) - 1;
    if (len > RFID_MAX_DATA_LEN) len = RFID_MAX_DATA_LEN;

    for (size_t i = 0; i < len; i++) {
        out[i] = chars[random(nChars)];
    }
    out[len] = '\0';
}

void rfid_printUid(const uint8_t *uid, uint8_t uidLen)
{
    printHexInternal(uid, uidLen);
}

