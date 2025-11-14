#include "rfid.h"
#include <Wire.h>

// ====== INSTANȚĂ PN532 ======
Adafruit_PN532 rfid_nfc(RFID_SDA_PIN, RFID_SCL_PIN);
RFID_Tag rfid_tag;

// ====== FUNCȚII ======
void rfid_init(void) {
    Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN);
    rfid_nfc.begin();
    uint32_t ver = rfid_nfc.getFirmwareVersion();
    if (!ver) {
        Serial.println("❌ PN532 nu raspunde!");
        while (1) delay(1000);
    }
    Serial.print("✅ PN532 detectat, versiune 0x");
    Serial.println((ver >> 16) & 0xFFFF, HEX);
    rfid_nfc.SAMConfig();
    Serial.println("🔹 RFID initializat si gata de lucru.");
}

bool rfid_detect(void) {
    if (!rfid_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                      rfid_tag.uid,
                                      &rfid_tag.uid_len))
        return false;

    // DETECTIE SIMPLA SI SIGURA
    if (rfid_tag.uid_len == 4) {
        rfid_tag.type = RFID_TYPE_MIFARE_1K;
    }
    else if (rfid_tag.uid_len == 7) {
        rfid_tag.type = RFID_TYPE_NTAG;
    }
    else {
        rfid_tag.type = RFID_TYPE_UNKNOWN;
    }

    return true;
}

void rfid_printUID(void) {
    Serial.print("💡 UID: ");
    for (uint8_t i = 0; i < rfid_tag.uid_len; i++) {
        if (rfid_tag.uid[i] < 0x10) Serial.print("0");
        Serial.print(rfid_tag.uid[i], HEX);
        if (i < rfid_tag.uid_len - 1) Serial.print(":");
    }
    Serial.println();
    Serial.print("📀 Tip tag: ");
    Serial.println(rfid_getTypeName(rfid_tag.type));
}

const char* rfid_getTypeName(uint8_t type) {
    switch (type) {
        case RFID_TYPE_MIFARE_1K:  return "MIFARE Classic 1K";
        case RFID_TYPE_ULTRALIGHT: return "MIFARE Ultralight";
        case RFID_TYPE_NTAG:       return "NTAG 21x";
        default:                   return "Necunoscut";
    }
}

// --- FUNCȚII MIFARE ---
bool rfid_authBlock(uint8_t block) {
    uint8_t keyA[6] = {RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT,
                       RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT};
    return rfid_nfc.mifareclassic_AuthenticateBlock(rfid_tag.uid, rfid_tag.uid_len, block, 0, keyA);
}

bool rfid_readBlock(uint8_t block, uint8_t* data16) {
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) {
        Serial.println("⚠️ Citirea directă e suportată doar pentru MIFARE Classic!");
        return false;
    }
    if (!rfid_authBlock(block)) {
        Serial.println("❌ Autentificare eșuată.");
        return false;
    }
    if (!rfid_nfc.mifareclassic_ReadDataBlock(block, data16)) {
        Serial.println("❌ Citire bloc eșuată.");
        return false;
    }
    return true;
}

bool rfid_writeBlock(uint8_t block, const uint8_t* data16) {
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) {
        Serial.println("⚠️ Scrierea directă e suportată doar pentru MIFARE Classic!");
        return false;
    }
    if (!rfid_authBlock(block)) {
        Serial.println("❌ Autentificare eșuată.");
        return false;
    }
    if (!rfid_nfc.mifareclassic_WriteDataBlock(block, (uint8_t*)data16)) {
        Serial.println("❌ Scriere bloc eșuată.");
        return false;
    }
    return true;
}
