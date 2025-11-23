#pragma once
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <ArduinoJson.h>

#include <Crypto.h>
#include <SHA256.h>


// === PINI PN532 I2C (tu deja le ai) ===
#define RFID_SDA_PIN   8
#define RFID_SCL_PIN   9

// tipuri tag
#define RFID_TYPE_UNKNOWN     0
#define RFID_TYPE_MIFARE_1K   1
#define RFID_TYPE_ULTRALIGHT  2
#define RFID_TYPE_NTAG_21x    3

// cheia default (0xFF sau 0x00 – tu folosești 0xFF sau 0x00, aici rămânem pe 0xFF)
#define RFID_KEY_DEFAULT      0xFF

// câtă dată RAW vrei să scrii/citești high-level
#define RFID_MAX_DATA         48   // 3 blocuri MIFARE (4,5,6) sau 12 pagini NTAG (4..15)

typedef struct {
    uint8_t  uid[7];
    uint8_t  uid_len;
    uint16_t atqa;
    uint8_t  sak;
    uint8_t  type;
} RFID_Tag;


struct ParsedTagData {
    String owner;
    String created;
    String encrypted_info;
    bool valid = false;
};



extern Adafruit_PN532 rfid_nfc;
extern RFID_Tag       rfid_tag;

// init + low-level
void rfid_init(void);
bool rfid_detect(void);
void rfid_printUID(void);

bool rfid_readBlock(uint8_t block, uint8_t* data16);
bool rfid_writeBlock(uint8_t block, const uint8_t* data16);
void rfid_printDecodedBlock(const uint8_t* data);

// === HIGH-LEVEL ===
// citește un tag (MIFARE sau NTAG), returnează UID hex și data RAW decodificată ca text
bool rfid_readTag(String &uidHex, String &data);

// scrie data RAW (string) pe tag (MIFARE sau NTAG), pornind de la zona user:
//  - MIFARE: blocurile 4,5,6,... (sar peste trailer)
//  - NTAG  : paginile 4.. (4 bytes/pagină)
bool rfid_writeTag(const String &data);

String buildPlaintextForEncryption(const String& infoResponse);
ParsedTagData parsePlaintext(const String& plaintext);


String serverToTagPlaintext(const String& jsonResponse);

static const uint8_t DEVICE_HALF_KEY[16] = {
    0x13, 0x27, 0x55, 0xA9,
    0xFE, 0x88, 0xC3, 0x91,
    0x42, 0x16, 0x7B, 0xD1,
    0x02, 0x99, 0xAB, 0xF0
};

static const uint8_t DEFAULT_FALLBACK_KEY[16] = {
    0x7A, 0xC1, 0x3E, 0x8F,
    0x55, 0xD4, 0x99, 0xA2,
    0x0B, 0x6E, 0xC7, 0x21,
    0xF3, 0x48, 0xDA, 0x15
};


 extern String GLOBAL_ACTIVE_SERVER_KEY;
