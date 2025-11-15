#include "rfid.h"
#include <Wire.h>

// buffer global PN532 din librărie
extern byte pn532_packetbuffer[];

Adafruit_PN532 rfid_nfc(RFID_SDA_PIN, RFID_SCL_PIN);
RFID_Tag rfid_tag;

void rfid_init(void)
{
    Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN);
    Wire.setClock(100000);

    rfid_nfc.begin();

    uint32_t ver = rfid_nfc.getFirmwareVersion();
    if (!ver) {
        Serial.println("❌ PN532 nu raspunde!");
        while (1) delay(1000);
    }

    Serial.print("✅ PN532 detectat, versiune 0x");
    Serial.println((ver >> 16) & 0xFFFF, HEX);

    rfid_nfc.SAMConfig();
    Serial.println("🔹 RFID initializat.");
}

bool rfid_detect(void)
{
    if (!rfid_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                      rfid_tag.uid,
                                      &rfid_tag.uid_len))
        return false;

    uint16_t atqa = ((uint16_t)pn532_packetbuffer[9] << 8) | pn532_packetbuffer[10];
    uint8_t sak  = pn532_packetbuffer[11];

    rfid_tag.atqa = atqa;
    rfid_tag.sak  = sak;

    if (sak == 0x08) {
        rfid_tag.type = RFID_TYPE_MIFARE_1K;
    }
    else if (sak == 0x00 && rfid_tag.uid_len == 7) {
        rfid_tag.type = RFID_TYPE_NTAG_21x;
    }
    else if (sak == 0x00) {
        rfid_tag.type = RFID_TYPE_ULTRALIGHT;
    }
    else {
        rfid_tag.type = RFID_TYPE_UNKNOWN;
    }

    return true;
}

void rfid_printUID(void)
{
    Serial.print("💡 UID: ");
    for (uint8_t i = 0; i < rfid_tag.uid_len; i++) {
        if (rfid_tag.uid[i] < 0x10) Serial.print("0");
        Serial.print(rfid_tag.uid[i], HEX);
        if (i < rfid_tag.uid_len - 1) Serial.print(":");
    }
    Serial.println();

    Serial.print("📀 Tip tag: ");
    switch (rfid_tag.type) {
        case RFID_TYPE_MIFARE_1K: Serial.println("MIFARE Classic 1K"); break;
        case RFID_TYPE_ULTRALIGHT: Serial.println("MIFARE Ultralight"); break;
        case RFID_TYPE_NTAG_21x: Serial.println("NTAG 21x"); break;
        default: Serial.println("Necunoscut"); break;
    }

    Serial.print("🔹 ATQA: 0x");
    Serial.println(rfid_tag.atqa, HEX);
    Serial.print("🔹 SAK: 0x");
    Serial.println(rfid_tag.sak, HEX);
}

// ------------------- MIFARE FUNCTIONS -------------------

static bool rfid_authBlock(uint8_t block)
{
    uint8_t keyA[6] = {RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT,
                       RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT};

    return rfid_nfc.mifareclassic_AuthenticateBlock(
                        rfid_tag.uid,
                        rfid_tag.uid_len,
                        block,
                        0,      // 0 = KEY A
                        keyA);
}

bool rfid_readBlock(uint8_t block, uint8_t* data16)
{
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) {
        Serial.println("⚠️ Nu este MIFARE Classic.");
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

bool rfid_writeBlock(uint8_t block, const uint8_t* data16)
{
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) {
        Serial.println("⚠️ Nu este MIFARE Classic.");
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



void rfid_printDecodedBlock(const uint8_t* data)
{
    Serial.print("🔎 Decodificat: ");

    for (int i = 0; i < 16; i++) {
        uint8_t b = data[i];

        // Caracter ASCII printabil?
        if (b >= 32 && b <= 126) {
            Serial.print((char)b);
        } else {
            Serial.print("\\x");
            if (b < 0x10) Serial.print("0");
            Serial.print(b, HEX);
        }
    }

    Serial.println();
}
