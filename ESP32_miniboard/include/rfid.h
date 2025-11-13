#ifndef RFID_H
#define RFID_H

#include <Arduino.h>

#define RFID_MAX_DATA_LEN 128

// tipul de date scrise pe card
typedef enum
{
    RFID_DATA_TYPE_STRING = 0,   // foloseste sirul dat ca atare
    RFID_DATA_TYPE_RANDOM = 1    // ignora "data" si genereaza intern un cod random de 128 caractere
} rfid_data_type_t;

// struct cu informatii despre tag
typedef struct
{
    uint8_t uid[7];
    uint8_t uidLen;
    char    data[RFID_MAX_DATA_LEN + 1];  // ASCII, terminat cu '\0'
    rfid_data_type_t dataType;
} rfid_tag_t;

/**
 * Initializeaza modulul RFID (PN532 pe I2C software).
 * Intoarce true daca a gasit PN532 si l-a configurat.
 */
bool rfid_init(uint8_t sdaPin, uint8_t sclPin);

/**
 * Citeste un tag (asteapta pana la timeoutMs pentru aparitia unui card).
 * Umple structura rfid_tag_t cu UID + data din blocurile 4..11.
 * Intoarce true daca citirea a reusit.
 */
bool rfid_readTag(rfid_tag_t *tag, uint32_t timeoutMs);

/**
 * Scrie pe un tag.
 *  - data      : sirul care se doreste scris (folosit doar daca type == RFID_DATA_TYPE_STRING)
 *  - type      : tip de date (STRING sau RANDOM)
 *  - expectedUid / expectedUidLen:
 *        - daca sunt NULL / 0 => scrie pe orice tag prezentat
 *        - daca nu sunt NULL => scrie doar daca UID-ul cardului corespunde
 *
 * Intoarce true daca scrierea a reusit.
 */
bool rfid_writeTag(const char *data,
                   rfid_data_type_t type,
                   const uint8_t *expectedUid,
                   uint8_t expectedUidLen);

/**
 * Genereaza un sir random de lungime "len" (max 128) in buffer-ul "out".
 * Sirul este terminat cu '\0'.
 */
void rfid_generateRandomString(char *out, size_t len);

/**
 * Functie utila pentru debug: afiseaza UID-ul in hex pe Serial.
 */
void rfid_printUid(const uint8_t *uid, uint8_t uidLen);

#endif // RFID_H
