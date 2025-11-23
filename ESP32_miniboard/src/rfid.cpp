#include "rfid.h"
#include <Wire.h>


// buffer global PN532 din librărie
extern byte pn532_packetbuffer[];

Adafruit_PN532 rfid_nfc(RFID_SDA_PIN, RFID_SCL_PIN);
RFID_Tag       rfid_tag;
String GLOBAL_ACTIVE_SERVER_KEY = "";

// ================== INIT ==================
void rfid_init(void)
{
    Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN);
    Wire.setClock(100000);   // I2C standard mode

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

// ================== DETECT ==================
bool rfid_detect(void)
{
    if (!rfid_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                      rfid_tag.uid,
                                      &rfid_tag.uid_len))
        return false;

    uint16_t atqa = ((uint16_t)pn532_packetbuffer[9] << 8) | pn532_packetbuffer[10];
    uint8_t  sak  = pn532_packetbuffer[11];

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

// mic helper intern: UID → string hex
static String rfid_uidToString()
{
    String s = "";
    for (uint8_t i = 0; i < rfid_tag.uid_len; i++) {
        if (rfid_tag.uid[i] < 0x10) s += "0";
        s += String(rfid_tag.uid[i], HEX);
        if (i < rfid_tag.uid_len - 1) s += ":";
    }
    s.toUpperCase();
    return s;
}

// ================== PRINT UID ==================
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

// ================== MIFARE LOW-LEVEL ==================

static bool rfid_authBlock(uint8_t block)
{
    uint8_t keyA[6] = {
        RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT,
        RFID_KEY_DEFAULT, RFID_KEY_DEFAULT, RFID_KEY_DEFAULT
    };

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

// ================== HELPER-E INTERNE HIGH-LEVEL ==================
// MIFARE: RAW read/write plecând de la blocul 4, sar peste trailer blocks (3,7,11,...)

static bool mifare_readRaw(uint8_t *outBuf, uint16_t maxLen, uint16_t &outLen)
{
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) return false;

    uint8_t  block  = 4;         // primul bloc de user data
    uint16_t offset = 0;
    uint8_t  tmp[16];

    while (offset < maxLen && block < 64) {

        // block-urile 3,7,11,... sunt trailer (ultimul din sector): sarim
        if ((block + 1) % 4 == 0) {
            block++;
            continue;
        }

        if (!rfid_readBlock(block, tmp)) {
            return false;
        }

        uint8_t toCopy = (maxLen - offset) >= 16 ? 16 : (maxLen - offset);
        memcpy(outBuf + offset, tmp, toCopy);
        offset += toCopy;
        block++;
    }

    outLen = offset;
    return true;
}

static bool mifare_writeRaw(const uint8_t *data, uint16_t len)
{
    if (rfid_tag.type != RFID_TYPE_MIFARE_1K) return false;

    uint8_t  block  = 4;  // primul bloc utilizabil
    uint16_t offset = 0;
    uint8_t  tmp[16];

    while (offset < len && block < 64) {

        // skip trailer blocks
        if ((block + 1) % 4 == 0) {
            block++;
            continue;
        }

        memset(tmp, 0, sizeof(tmp));
        uint8_t toCopy = (len - offset) >= 16 ? 16 : (len - offset);
        memcpy(tmp, data + offset, toCopy);

        if (!rfid_writeBlock(block, tmp)) {
            return false;
        }

        offset += toCopy;
        block++;
    }

    return (offset == len);
}

// NTAG / ULTRALIGHT RAW read/write: folosim pagini 4..15 (12 pagini × 4 bytes = 48 bytes)

static bool ntag_readRaw(uint8_t *outBuf, uint16_t maxLen, uint16_t &outLen)
{
    if (rfid_tag.type != RFID_TYPE_NTAG_21x &&
        rfid_tag.type != RFID_TYPE_ULTRALIGHT) {
        return false;
    }

    uint8_t pageBuf[4];
    uint8_t page = 4;    // user data începe de la 4
    uint16_t offset = 0;

    while (offset < maxLen && page < 16) { // 4..15
        if (!rfid_nfc.ntag2xx_ReadPage(page, pageBuf)) {
            Serial.println("❌ NTAG read page fail");
            return false;
        }

        for (int i = 0; i < 4 && offset < maxLen; i++) {
            outBuf[offset++] = pageBuf[i];
        }

        page++;
    }

    outLen = offset;
    return true;
}

static bool ntag_writeRaw(const uint8_t *data, uint16_t len)
{
    if (rfid_tag.type != RFID_TYPE_NTAG_21x &&
        rfid_tag.type != RFID_TYPE_ULTRALIGHT) {
        return false;
    }

    uint8_t pageBuf[4];
    uint8_t page = 4;    // user data
    uint16_t offset = 0;

    while (offset < len && page < 16) { // 4..15
        memset(pageBuf, 0, sizeof(pageBuf));
        uint8_t toCopy = (len - offset) >= 4 ? 4 : (len - offset);
        memcpy(pageBuf, data + offset, toCopy);

        if (!rfid_nfc.ntag2xx_WritePage(page, pageBuf)) {
            Serial.println("❌ NTAG write page fail");
            return false;
        }

        offset += toCopy;
        page++;
    }

    return (offset == len);
}
//====================CRIPT INFO=======================



void xteaEncrypt(uint32_t v[2], const uint32_t key[4]) {
    uint32_t v0 = v[0], v1 = v[1];
    uint32_t sum = 0;
    const uint32_t delta = 0x9E3779B9;

    for (uint32_t i = 0; i < 32; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

void xteaDecrypt(uint32_t v[2], const uint32_t key[4]) {
    uint32_t v0 = v[0], v1 = v[1];
    const uint32_t delta = 0x9E3779B9;
    uint32_t sum = delta * 32;

    for (uint32_t i = 0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

// ==========================
// KEY DERIVATION FUNCTION
// serverHalfKey + DEVICE_HALF_KEY -> SHA256 -> XTEA 128-bit key
// ==========================

void deriveKey(const String& serverKey, uint32_t outKey[4]) {
    SHA256 sha;

    sha.reset();
    sha.update((const uint8_t*)serverKey.c_str(), serverKey.length());
    sha.update(DEVICE_HALF_KEY, 16);

    uint8_t hash[32];
    sha.finalize(hash, sizeof(hash));

    // first 128 bits = XTEA key
    for (int i = 0; i < 4; i++) {
        outKey[i] = ((uint32_t)hash[i*4] << 24) |
                    ((uint32_t)hash[i*4+1] << 16) |
                    ((uint32_t)hash[i*4+2] << 8) |
                    (uint32_t)hash[i*4+3];
    }
}

// ==========================
// PUBLIC API
// ==========================
String buildPlaintextForEncryption(const String& infoResponse)
{
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, infoResponse)) {
        Serial.println("❌ JSON parse error");
        return "";
    }

    const char* status = doc["status"] | "error";
    if (strcmp(status, "ok") != 0) {
        Serial.println("❌ Status not OK");
        return "";
    }

    const char* encrypted_info = doc["encrypted_info"] | "";
    const char* owner = doc["owner"] | "N/A";
    const char* created = doc["created"] | "";

    // Build a flat string to encrypt
    String plaintext =
        "owner=" + String(owner) +
        ";created=" + String(created) +
        ";info=" + String(encrypted_info);

    return plaintext;
}




String encryptData(const String& plaintext, const String& serverKey)
{
    uint32_t key[4];
    deriveKey(serverKey, key);

    // pad to 8-byte blocks
    int len = plaintext.length();
    int padded = ((len + 7) / 8) * 8;

    uint8_t buf[padded];
    memset(buf, 0, padded);
    memcpy(buf, plaintext.c_str(), len);

    for (int i = 0; i < padded; i += 8) {
        uint32_t v[2];
        memcpy(v, buf + i, 8);
        xteaEncrypt(v, key);
        memcpy(buf + i, v, 8);
    }

    // return as binary string (not HEX)
    return String((char*)buf, padded);
}

String decryptData(const uint8_t* cipher, int len, const String& serverKey)
{
    uint32_t key[4];
    deriveKey(serverKey, key);

    uint8_t buf[len];
    memcpy(buf, cipher, len);

    for (int i = 0; i < len; i += 8) {
        uint32_t v[2];
        memcpy(v, buf + i, 8);
        xteaDecrypt(v, key);
        memcpy(buf + i, v, 8);
    }

    // trim possible padding zeros
    return String((char*)buf);
}





// ================== HIGH-LEVEL API ==================

// citește tag-ul (MIFARE/NTAG), întoarce UID hex + data RAW convertită la string
bool rfid_readTag(String &uidHex, String &decoded)
{
    if (!rfid_detect()) {
        return false;
    }

    uidHex = rfid_uidToString();
    decoded = "";

    uint8_t  buf[RFID_MAX_DATA];
    uint16_t len = 0;

    bool ok = false;

    if (rfid_tag.type == RFID_TYPE_MIFARE_1K) {
        ok = mifare_readRaw(buf, RFID_MAX_DATA, len);
    }
    else if (rfid_tag.type == RFID_TYPE_NTAG_21x ||
             rfid_tag.type == RFID_TYPE_ULTRALIGHT) {
        ok = ntag_readRaw(buf, RFID_MAX_DATA, len);
    } else {
        Serial.println("⚠️ Tip tag necunoscut → nu se poate citi RAW.");
        return false;
    }

    if (!ok) return false;

    // --- determinăm cheia ---
    String key((const char*)DEVICE_HALF_KEY, 16);

    if (key.length() == 0) key = String((char*)DEFAULT_FALLBACK_KEY, 16);

    // --- DECRIPTARE automată ---
    String decrypted = decryptData(buf, len, key);

    decoded = decrypted;
    return true;
}


// scrie stringul pe tag (MIFARE/NTAG), RAW (fără NDEF), în zona user
// scrie tag CRIPTAT automat
bool rfid_writeTag(const String &plaintext)
{
    if (!rfid_detect()) return false;

    // cheia device default
    String key((const char*)DEVICE_HALF_KEY, 16);

    // dacă serverul a furnizat cheie → o folosim
    if (GLOBAL_ACTIVE_SERVER_KEY.length() > 0)
        key = GLOBAL_ACTIVE_SERVER_KEY;

    // fallback dacă totuși nu e setată cheia
    if (key.length() == 0)
        key = String((const char*)DEFAULT_FALLBACK_KEY, 16);

    String encrypted = encryptData(plaintext, key);

    uint16_t len = encrypted.length();
    if (len > RFID_MAX_DATA) len = RFID_MAX_DATA;

    uint8_t buf[RFID_MAX_DATA];
    memcpy(buf, encrypted.c_str(), len);

    bool ok = false;

    if (rfid_tag.type == RFID_TYPE_MIFARE_1K)
        ok = mifare_writeRaw(buf, len);
    else if (rfid_tag.type == RFID_TYPE_NTAG_21x ||
             rfid_tag.type == RFID_TYPE_ULTRALIGHT)
        ok = ntag_writeRaw(buf, len);
    else
        return false;

    if (ok)
        Serial.println("✅ Scriere CRIPTATĂ reușită pe tag.");
    else
        Serial.println("❌ Scriere CRIPTATĂ eșuată.");

    return ok;
}

ParsedTagData parsePlaintext(const String& plaintext)
{
    ParsedTagData out;

    // Căutăm delimitatorii
    int p_owner = plaintext.indexOf("owner=");
    int p_created = plaintext.indexOf(";created=");
    int p_info = plaintext.indexOf(";info=");

    if (p_owner == -1 || p_created == -1 || p_info == -1) {
        Serial.println("❌ parsePlaintext: invalid format");
        return out;
    }

    // Extragem owner
    out.owner = plaintext.substring(p_owner + 6, p_created);

    // Extragem created
    out.created = plaintext.substring(p_created + 9, p_info);

    // Extragem encrypted_info
    out.encrypted_info = plaintext.substring(p_info + 6);

    out.valid = true;
    return out;
}




String serverToTagPlaintext(const String& jsonResponse)
{
    // parse JSON
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, jsonResponse)) {
        Serial.println("❌ serverToTagPlaintext: JSON parse error");
        return "";
    }

    const char* status = doc["status"] | "error";

    // dacă status nu e ok, nu generăm tag
    if (strcmp(status, "ok") != 0) {
        Serial.printf("❌ serverToTagPlaintext: status=%s (cannot build tag)\n", status);
        return "";
    }

    // extragem câmpurile necesare
    const char* owner = doc["owner"] | "";
    const char* created = doc["created"] | "";
    const char* encrypted_info = doc["encrypted_info"] | "";

    // construim stringul plain text exact ca pe tag
    String out = "owner=";
    out += owner;
    out += ";created=";
    out += created;
    out += ";info=";
    out += encrypted_info;

    return out;
}

