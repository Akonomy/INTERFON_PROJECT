#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

// ---- PIN SET ----
#define RFID_SDA_PIN 4
#define RFID_SCL_PIN 5
#define RFID_KEY_DEFAULT 0xFF

// ---- TIPURI ----
#define RFID_TYPE_UNKNOWN     0
#define RFID_TYPE_MIFARE_1K   1
#define RFID_TYPE_ULTRALIGHT  2
#define RFID_TYPE_NTAG_21x    3

typedef struct {
    uint8_t uid[7];
    uint8_t uid_len;
    uint16_t atqa;
    uint8_t sak;
    uint8_t type;
} RFID_Tag;

// ---- VARIABILE GLOBALE ----
extern RFID_Tag rfid_tag;
extern Adafruit_PN532 rfid_nfc;

// ---- FUNCȚII ----
void rfid_init(void);
bool rfid_detect(void);
void rfid_printUID(void);

bool rfid_readBlock(uint8_t block, uint8_t* data16);
bool rfid_writeBlock(uint8_t block, const uint8_t* data16);



void rfid_printDecodedBlock(const uint8_t* data) ;
#endif
