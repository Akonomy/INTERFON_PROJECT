#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

// === CONFIGURARE HARDWARE PN532 ===
#define RFID_SDA_PIN 8
#define RFID_SCL_PIN 9

// === DEFINIȚII TIP TAG ===
#define RFID_TYPE_UNKNOWN    0
#define RFID_TYPE_MIFARE_1K  1
#define RFID_TYPE_ULTRALIGHT 2
#define RFID_TYPE_NTAG       3

// === CONSTANTE MIFARE ===
#define RFID_BLOCK_SIZE 16
#define RFID_KEY_DEFAULT 0xFF

// === STRUCTURI ===
typedef struct {
    uint8_t uid[7];
    uint8_t uid_len;
    uint8_t type;
} RFID_Tag;

// === VARIABILE GLOBALE ===
extern RFID_Tag rfid_tag;
extern Adafruit_PN532 rfid_nfc;

// === FUNCȚII DE INTERFAȚĂ ===
void rfid_init(void);
bool rfid_detect(void);
void rfid_printUID(void);
const char* rfid_getTypeName(uint8_t type);
bool rfid_authBlock(uint8_t block);
bool rfid_readBlock(uint8_t block, uint8_t* data16);
bool rfid_writeBlock(uint8_t block, const uint8_t* data16);

#endif
