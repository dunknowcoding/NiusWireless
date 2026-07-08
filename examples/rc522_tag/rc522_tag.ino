/*
 * rc522_tag — Full tag operations, auto-adapts to detected card type
 *
 * The sketch detects the card family and runs the right operations:
 *
 *   MIFARE Classic 1K / 4K / Mini (S50 / keychain / fob):
 *     0. CUID UID-change demo (Chinese "magic" cards that accept a
 *        standard WRITE to block 0 via the CUID backdoor)
 *     1. Full 64-block memory dump (key-by-key authentication)
 *     2. Write a data block and read it back to verify
 *     3. Format a value block (signed 32-bit value with backup copy)
 *     4. Authenticate with Key A and Key B on the same sector
 *     5. Change Key A in a sector trailer
 *     6. Cleanup (halt, wait for tag to leave)
 *
 *   MIFARE Ultralight / NTAG / Ultralight EV1:
 *     0. Read all pages (UID + manufacturer data + user pages)
 *     1. Write a test page and read it back
 *     2. Cleanup
 *
 *   Other types (ISO 14443-4 / DESFire / ISO 18092 / Plus / unknown):
 *     "this library does not operate on this family — see the suggested
 *      tool" message, then waits for the tag to leave the field.
 *
 * Use sector 1 (blocks 4–7) for every Classic write so the manufacturer
 * block in sector 0 and the keychain's UID are never touched — except in
 * the CUID step 0, which deliberately rewrites block 0 to change the UID.
 *
 * If you know the 12-hex-digit key for your keychain, type it on Serial
 * during setup() and press Enter — the sketch will use it before falling
 * back to the built-in dictionary. Press Enter alone to skip.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)
 *   RC522 SCK  -> SCL  (D19 / A5)
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 IRQ  -> D13  (not used)
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 */

#include <NiusWireless.h>

// --- Pin definitions (SDA/SCL macros keep the sketch portable) ---
#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12

// Sector 1 is the playground — never touches the manufacturer block.
#define TEST_DATA_BLOCK     4
#define TEST_KEYB_BLOCK     5
#define TEST_VALUE_BLOCK    6
#define TEST_TRAILER_BLOCK  7

// Some keychains ship with non-factory keys. tryKeys[] is a dictionary of
// well-known MIFARE Classic keys; the sketch tries each in order on every
// sector. The first key that authenticates is cached and reused so the
// rest of the run doesn't have to re-discover it.
//
// >>> If you know the key for your keychain, put it at the top of the
//     list — it will be tried first. <<<
const uint8_t tryKeys[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // NXP factory default
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},  // NXP MAD key A
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},  // NXP MAD key B
    {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5},
    {0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},  // NXP public transport
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // all-zero
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77, 0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99, 0x99, 0x99, 0x99},
    {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
    {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB},
    {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
    {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD},
    {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE},
    {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0},
    {0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1},
    {0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5},
    {0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A},
    {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
    {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x7B, 0xF3, 0x52, 0x21, 0xA4, 0x68},
    {0x71, 0x4C, 0x49, 0x49, 0x52, 0x45},
    {0x96, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5},
    {0xB3, 0x96, 0xCD, 0x67, 0xA9, 0x6C},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
    {0x55, 0x44, 0x33, 0x22, 0x11, 0x00},
    {0xFE, 0xED, 0xFA, 0xCE, 0xBE, 0xEF},
};
const uint8_t tryKeysCount = sizeof(tryKeys) / sizeof(tryKeys[0]);

// Cached key for the current sector — filled in by tryAuthenticate().
uint8_t workingKey[6];

// Optional user-supplied key (entered on Serial in setup()).
uint8_t userKey[6];
bool    useUserKey = false;

// Replacement Key A written to the sector 1 trailer in step 5
uint8_t newKey[6]     = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};

// 16-byte payload for the data-block write step
uint8_t writeData[16] = {
    'N','i','u','s','L','a','b',' ',
    'R','C','5','2','2',' ','O','K'
};

NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

// ---------------------------------------------------------------------------
// Helpers — printing
// ---------------------------------------------------------------------------

// Print one 16-byte block as hex (left side) + ASCII (right side).
void printBlock(uint8_t *data) {
    for (uint8_t i = 0; i < 16; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(i == 7 ? "  " : " ");
    }
    Serial.print(" |");
    for (uint8_t i = 0; i < 16; i++) {
        char c = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : '.';
        Serial.print(c);
    }
    Serial.println('|');
}

// Print one 6-byte MIFARE key as upper-case hex.
void printKey(uint8_t *key) {
    for (uint8_t i = 0; i < 6; i++) {
        if (key[i] < 0x10) Serial.print('0');
        Serial.print(key[i], HEX);
    }
}

// Print one 4-byte Ultralight/NTAG page as hex + ASCII.
void printPage4(uint8_t *data) {
    for (uint8_t i = 0; i < 4; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.print(" |");
    for (uint8_t i = 0; i < 4; i++) {
        char c = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : '.';
        Serial.print(c);
    }
    Serial.println('|');
}

void printLine() {
    Serial.println(F("---------------------------------------------------------------"));
}

// ---------------------------------------------------------------------------
// Helpers — MIFARE Classic auth
// ---------------------------------------------------------------------------

// tryAuthenticate() — try userKey (if set) first, then every key in
// tryKeys[] until one works. On success, copies the winning key into
// workingKey[] and returns true.
bool tryAuthenticate(uint8_t trailerBlock, uint8_t keyType) {
    if (useUserKey) {
        if (rfid.authenticate(trailerBlock, keyType, userKey) == NIUS_OK) {
            memcpy(workingKey, userKey, 6);
            rfid.stopCrypto();
            return true;
        }
        rfid.stopCrypto();
    }
    for (uint8_t i = 0; i < tryKeysCount; i++) {
        if (rfid.authenticate(trailerBlock, keyType, (uint8_t *)tryKeys[i]) == NIUS_OK) {
            memcpy(workingKey, tryKeys[i], 6);
            rfid.stopCrypto();
            return true;
        }
        rfid.stopCrypto();
    }
    return false;
}

// promptUserKey() — ask the user to type a 12-hex-digit key on Serial.
// Waits up to 5 seconds for input. The user can just press Enter (or wait)
// to skip.
void promptUserKey() {
    Serial.println();
    Serial.println(F("If you know the 12-hex-digit key for this tag, type it now"));
    Serial.println(F("(e.g. AABBCCDDEEFF) and press Enter. Press Enter alone to use"));
    Serial.println(F("the built-in dictionary. Waiting 5 seconds..."));

    String input = "";
    unsigned long start = millis();
    while ((millis() - start) < 5000UL) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') { break; }
            if (input.length() < 12) { input += c; }
        }
    }

    if (input.length() != 12) {
        Serial.println(F("No key typed — using built-in dictionary."));
        return;
    }
    for (uint8_t i = 0; i < 6; i++) {
        char hex[3] = {input[i*2], input[i*2+1], 0};
        char *endp;
        long v = strtol(hex, &endp, 16);
        if (*endp != 0 || v < 0 || v > 0xFF) {
            Serial.println(F("Invalid hex — using built-in dictionary."));
            return;
        }
        userKey[i] = (uint8_t)v;
    }
    useUserKey = true;
    Serial.print(F("Using user key: ")); printKey(userKey); Serial.println();
}

// ---------------------------------------------------------------------------
// MIFARE Classic flow
// ---------------------------------------------------------------------------

// Step 0: CUID UID-change demo. Chinese "magic" CUID cards accept a
// standard MIFARE Classic WRITE to block 0 via a backdoor, regardless of
// the auth state. The new UID shows up on the next scan.
static void classicStep0_cuidDemo() {
    Serial.println();
    printLine();
    Serial.println(F("0. CUID UID-change demo"));
    printLine();

    uint8_t curUid[NIUS_UID_MAX_LEN];
    uint8_t curUidLen = 0;
    rfid.getUIDBytes(curUid, curUidLen);

    if (curUidLen != 4) {
        Serial.println(F("  UID is not 4 bytes — skipping CUID demo."));
        return;
    }

    uint8_t newUid[4] = {curUid[0], curUid[1], curUid[2],
                         (uint8_t)(curUid[3] + 1)};
    uint8_t newBcc = newUid[0] ^ newUid[1] ^ newUid[2] ^ newUid[3];

    Serial.print(F("  Current UID: "));
    for (uint8_t i = 0; i < 4; i++) {
        if (curUid[i] < 0x10) Serial.print('0');
        Serial.print(curUid[i], HEX);
    }
    Serial.println();
    Serial.print(F("  New UID:     "));
    for (uint8_t i = 0; i < 4; i++) {
        if (newUid[i] < 0x10) Serial.print('0');
        Serial.print(newUid[i], HEX);
    }
    Serial.println();

    // Build new block 0: [newUid 4B][BCC 1B][manufacturer data 11B]
    uint8_t newBlock0[16] = {0};
    memcpy(newBlock0, newUid, 4);
    newBlock0[4] = newBcc;

    uint8_t defKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (rfid.authenticate(3, NIUS_KEY_A, defKey) != NIUS_OK) {
        Serial.println(F("  Auth to sector 0 FAILED — skipping CUID demo."));
        return;
    }
    uint8_t r = rfid.writeBlock(0, newBlock0);
    Serial.print(F("  writeBlock(0) result: "));
    Serial.println(r == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();
    if (r == NIUS_OK) {
        Serial.println(F("  Note: some CUID cards re-announce as Ultralight after"));
        Serial.println(F("  a block-0 write. Re-place the card to see the new UID,"));
        Serial.println(F("  or the next loop iteration will pick it up."));
    }
}

// Step 1: full 64-block memory dump.
static void classicStep1_fullDump() {
    Serial.println();
    printLine();
    Serial.println(F("1. Full 64-block memory dump"));
    printLine();

    uint8_t okSectors = 0;
    for (uint8_t s = 0; s < 16; s++) {
        uint8_t trailer = s * 4 + 3;
        Serial.print(F("  Sector ")); Serial.print(s);
        Serial.print(F("  (trailer ")); Serial.print(trailer); Serial.println(')');

        if (!tryAuthenticate(trailer, NIUS_KEY_A)) {
            Serial.println(F("    No known key matched — skipping sector."));
            Serial.println(F("    (edit tryKeys[] in the source if you know the key)"));
            continue;
        }
        okSectors++;
        Serial.print(F("    Key used: ")); printKey(workingKey); Serial.println();

        for (uint8_t b = 0; b < 4; b++) {
            uint8_t block = s * 4 + b;
            uint8_t buf[16];
            if (rfid.readBlock(block, buf) == NIUS_OK) {
                Serial.print(F("    Block "));
                if (block < 10) Serial.print(' ');
                Serial.print(block); Serial.print(F(": "));
                printBlock(buf);
            }
        }
        rfid.stopCrypto();
    }
    Serial.print(F("  Authenticated sectors: "));
    Serial.print(okSectors); Serial.println(F(" / 16"));
}

// Step 2: write a data block and verify.
static void classicStep2_writeDataBlock() {
    Serial.println();
    printLine();
    Serial.println(F("2. Write data block and read back"));
    printLine();

    if (!tryAuthenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A)) {
        Serial.println(F("  No known key matched — skipping write."));
        return;
    }
    Serial.print(F("  Write to block ")); Serial.print(TEST_DATA_BLOCK);
    Serial.print(F(" (key=")); printKey(workingKey); Serial.println(F(")"));
    Serial.print(F("    Payload: ")); printBlock(writeData);

    uint8_t status = rfid.writeBlock(TEST_DATA_BLOCK, writeData);
    Serial.print(F("  Write result: "));
    Serial.println(status == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();

    if (rfid.authenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A, workingKey) != NIUS_OK) {
        Serial.println(F("  Re-auth FAILED — skipping read-back."));
        return;
    }
    uint8_t readData[16];
    if (rfid.readBlock(TEST_DATA_BLOCK, readData) == NIUS_OK) {
        Serial.print(F("  Read back:     ")); printBlock(readData);
        bool match = true;
        for (uint8_t i = 0; i < 16; i++) {
            if (readData[i] != writeData[i]) { match = false; break; }
        }
        Serial.print(F("  Verify: "));
        Serial.println(match ? F("PASS — data matches.")
                             : F("FAIL — data mismatch!"));
    }
    rfid.stopCrypto();
}

// Step 3: write a value block.
static void classicStep3_valueBlock() {
    Serial.println();
    printLine();
    Serial.println(F("3. Format a value block"));
    printLine();
    Serial.println(F("  Layout: value | ~value | value | addr | ~addr | addr | ~addr"));

    int32_t value = 100;
    uint8_t vb[16];
    vb[0]  = (uint8_t)(value & 0xFF);
    vb[1]  = (uint8_t)((value >> 8)  & 0xFF);
    vb[2]  = (uint8_t)((value >> 16) & 0xFF);
    vb[3]  = (uint8_t)((value >> 24) & 0xFF);
    vb[4]  = (uint8_t)~vb[0];
    vb[5]  = (uint8_t)~vb[1];
    vb[6]  = (uint8_t)~vb[2];
    vb[7]  = (uint8_t)~vb[3];
    vb[8]  = vb[0];  vb[9]  = vb[1];  vb[10] = vb[2];  vb[11] = vb[3];
    vb[12] = TEST_VALUE_BLOCK;
    vb[13] = (uint8_t)~TEST_VALUE_BLOCK;
    vb[14] = TEST_VALUE_BLOCK;
    vb[15] = (uint8_t)~TEST_VALUE_BLOCK;

    if (!tryAuthenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A)) {
        Serial.println(F("  No known key matched — skipping value block."));
        return;
    }
    Serial.print(F("  Write value block (value="));
    Serial.print(value);
    Serial.print(F(", addr=")); Serial.print(TEST_VALUE_BLOCK);
    Serial.println(F(")"));
    Serial.print(F("    Payload: ")); printBlock(vb);

    uint8_t status = rfid.writeBlock(TEST_VALUE_BLOCK, vb);
    Serial.print(F("  Write result: "));
    Serial.println(status == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();

    if (rfid.authenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A, workingKey) == NIUS_OK) {
        uint8_t readBack[16];
        if (rfid.readBlock(TEST_VALUE_BLOCK, readBack) == NIUS_OK) {
            Serial.print(F("  Read back:     ")); printBlock(readBack);
            int32_t decoded = (int32_t)((uint32_t)readBack[0]
                              | ((uint32_t)readBack[1] << 8)
                              | ((uint32_t)readBack[2] << 16)
                              | ((uint32_t)readBack[3] << 24));
            Serial.print(F("  Decoded value: ")); Serial.println(decoded);
        }
        rfid.stopCrypto();
    }
}

// Step 4: authenticate with Key A and Key B.
static void classicStep4_keyAB() {
    Serial.println();
    printLine();
    Serial.println(F("4. Authenticate with Key A and Key B"));
    printLine();

    if (!tryAuthenticate(TEST_KEYB_BLOCK, NIUS_KEY_A)) {
        Serial.println(F("  No known key matched — skipping Key A/B auth test."));
        return;
    }
    Serial.print(F("  Key A: "));
    uint8_t ra = rfid.authenticate(TEST_KEYB_BLOCK, NIUS_KEY_A, workingKey);
    Serial.println(ra == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();

    Serial.print(F("  Key B: "));
    uint8_t rb = rfid.authenticate(TEST_KEYB_BLOCK, NIUS_KEY_B, workingKey);
    Serial.println(rb == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();
}

// Step 5: change Key A.
static void classicStep5_changeKeyA() {
    Serial.println();
    printLine();
    Serial.println(F("5. Change Key A on sector 1"));
    printLine();

    if (!tryAuthenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A)) {
        Serial.println(F("  No known key matched — skipping key change."));
        return;
    }
    Serial.print(F("  Old Key A: ")); printKey(workingKey); Serial.println();
    Serial.print(F("  New Key A: ")); printKey(newKey);     Serial.println();

    uint8_t trailer[16];
    uint8_t status = rfid.readBlock(TEST_TRAILER_BLOCK, trailer);
    if (status != NIUS_OK) {
        Serial.println(F("  Read trailer FAILED — skipping key change."));
        rfid.stopCrypto();
        return;
    }
    uint8_t access[4], keyB[6];
    memcpy(access, &trailer[6], 4);
    memcpy(keyB,   &trailer[10], 6);
    memcpy(&trailer[0],  newKey,  6);
    memcpy(&trailer[6],  access,  4);
    memcpy(&trailer[10], keyB,    6);

    status = rfid.writeBlock(TEST_TRAILER_BLOCK, trailer);
    Serial.print(F("  Write new trailer: "));
    Serial.println(status == NIUS_OK ? F("OK") : F("FAILED"));
    rfid.stopCrypto();

    if (rfid.authenticate(TEST_TRAILER_BLOCK, NIUS_KEY_A, newKey) == NIUS_OK) {
        Serial.println(F("  Auth with NEW Key A: OK (key change verified)."));
        uint8_t readBack[16];
        if (rfid.readBlock(TEST_DATA_BLOCK, readBack) == NIUS_OK) {
            Serial.print(F("  Read block 4 with new key: ")); printBlock(readBack);
        }
    } else {
        Serial.println(F("  Auth with NEW Key A: FAILED."));
    }
    rfid.stopCrypto();
}

// Step 6: cleanup — halt the card, wait for it to leave the field.
static void classicStep6_cleanup() {
    Serial.println();
    printLine();
    Serial.println(F("6. Cleanup"));
    printLine();
    rfid.halt();
    Serial.println(F("  Keychain halted. Remove it to repeat."));
    Serial.println();

    uint32_t waitStart = millis();
    while ((millis() - waitStart) < 5000UL) {
        if (!rfid.cardPresentWake()) { break; }
        rfid.halt();
        delay(150);
    }
    delay(500);
}

static void runClassicFlow() {
    classicStep0_cuidDemo();
    classicStep1_fullDump();
    classicStep2_writeDataBlock();
    classicStep3_valueBlock();
    classicStep4_keyAB();
    classicStep5_changeKeyA();
    classicStep6_cleanup();
}

// ---------------------------------------------------------------------------
// MIFARE Ultralight / NTAG flow
// ---------------------------------------------------------------------------

// Step 0: read all pages (UID + manufacturer data + user pages).
// Page 0 READ returns pages 0-3, page 4 returns 4-7, etc. Stop on NAK.
static void ultralightStep0_readAll() {
    Serial.println();
    printLine();
    Serial.println(F("0. Read all pages"));
    printLine();

    // Standard layout for small Ultralight / NTAGs:
    //   Pages 0-1: UID
    //   Page 2:    internal / lock bytes
    //   Page 3:    CC (CC for NTAG, internal for UL)
    //   Page 4+:   user data
    //   Last 4 pages: lock / config / password
    for (uint8_t p = 0; p < 64; p += 4) {
        uint8_t buf[16];
        uint8_t r = rfid.readPage(p, buf);
        if (r != NIUS_OK) {
            Serial.print(F("  Pages "));
            if (p < 10) Serial.print(' ');
            Serial.print(p);
            if (p == 0) {
                Serial.println(F(": NAK (page 0 unreadable — tag may be in"));
                Serial.println(F("      a broken state, e.g. a CUID card after a"));
                Serial.println(F("      block-0 write that didn't take cleanly)."));
            } else {
                Serial.println(F(": NAK (end of memory)"));
            }
            break;
        }
        Serial.print(F("  Pages "));
        if (p < 10) Serial.print(' ');
        Serial.print(p);
        Serial.print(F("-"));
        if ((p + 3) < 10) Serial.print(' ');
        Serial.print(p + 3);
        Serial.print(F(": "));
        printBlock(buf);
    }
}

// Step 1: write a single user page and read it back.
// Uses page 4 (first user page on most NTAGs) as a safe test target.
static void ultralightStep1_writePage() {
    Serial.println();
    printLine();
    Serial.println(F("1. Write page 4 and read back"));
    printLine();

    uint8_t payload[4] = {'N', 'I', 'U', 'S'};
    Serial.print(F("  Write to page 4: "));
    printPage4(payload);
    uint8_t r = rfid.writePage(4, payload);
    Serial.print(F("  Write result: "));
    Serial.println(r == NIUS_OK ? F("OK") : F("FAILED"));

    if (r == NIUS_OK) {
        uint8_t readBack[16];
        if (rfid.readPage(4, readBack) == NIUS_OK) {
            Serial.print(F("  Read back pages 4-7: "));
            printBlock(readBack);
        }
    } else {
        Serial.println(F("  (Some CUID / NTAG cards reject writes from raw"));
        Serial.println(F("   READ/WRITE 0x30/0xA2 commands. Use MCT on Android"));
        Serial.println(F("   or a Proxmark3 to write to this card.)"));
    }
}

static void runUltralightFlow() {
    ultralightStep0_readAll();
    ultralightStep1_writePage();
    rfid.halt();
    Serial.println();
    printLine();
    Serial.println(F("Ultralight flow complete. Halted."));
    printLine();

    uint32_t waitStart = millis();
    while ((millis() - waitStart) < 5000UL) {
        if (!rfid.cardPresentWake()) { break; }
        rfid.halt();
        delay(150);
    }
    delay(500);
}

// ---------------------------------------------------------------------------
// Unsupported / unknown flow
// ---------------------------------------------------------------------------

static void runUnsupportedFlow() {
    switch (rfid.getCardType()) {
        case NIUS_CARD_ISO14443_4:
        case NIUS_CARD_DESFIRE:
            Serial.println(F("  This tag is ISO 14443-4 / DESFire. NiusWireless does not"));
            Serial.println(F("  implement APDU-level DESFire operations — use a PN532"));
            Serial.println(F("  or an Android app (MCT / TagInfo)."));
            break;
        case NIUS_CARD_ISO18092:
            Serial.println(F("  This is an ISO 18092 (NFC-IP1) peer — not a readable tag."));
            break;
        case NIUS_CARD_MIFARE_PLUS:
            Serial.println(F("  MIFARE Plus detected. The Classic emulation layer may work,"));
            Serial.println(F("  but security level 3 needs AES and is not implemented."));
            break;
        default:
            Serial.println(F("  Unknown type. No NiusWireless flow matches."));
            break;
    }

    rfid.halt();
    uint32_t waitStart = millis();
    while ((millis() - waitStart) < 10000UL) {
        if (!rfid.cardPresentWake()) { break; }
        rfid.halt();
        delay(150);
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(9600);
    delay(1500);  // Give USB CDC time to enumerate on UNO R4 WiFi

    Serial.println(F("NiusWireless — RC522 Tag Operations (auto-adapts to card type)"));
    Serial.println(F("==================================================================="));

    if (!rfid.begin()) {
        Serial.println(F("ERROR: RC522 not found. Check wiring and power."));
        while (1) { delay(500); }
    }
    Serial.print(F("Reader ready: "));
    Serial.println(rfid.getVersion());
    Serial.println(F("Place a tag on the reader..."));
    promptUserKey();
    Serial.println();
}

// ---------------------------------------------------------------------------
// Loop — detect, show card info, dispatch to the right flow
// ---------------------------------------------------------------------------

void loop() {
    if (!rfid.cardPresentWake()) { return; }

    Serial.println();
    printLine();
    Serial.println(F("Tag detected"));
    printLine();
    Serial.print(F("  UID:   ")); Serial.println(rfid.getUID());
    Serial.print(F("  ATQA:  ")); Serial.println(rfid.getATQA());
    Serial.print(F("  SAK:   0x"));
    if (rfid.getSAK() < 0x10) Serial.print('0');
    Serial.println(rfid.getSAK(), HEX);
    Serial.print(F("  Type:  ")); Serial.println(rfid.getCardTypeName());
    Serial.println();

    switch (rfid.getCardType()) {
        case NIUS_CARD_MIFARE_1K:
        case NIUS_CARD_MIFARE_4K:
        case NIUS_CARD_MIFARE_MINI:
            runClassicFlow();
            break;
        case NIUS_CARD_MIFARE_UL:
            runUltralightFlow();
            break;
        default:
            runUnsupportedFlow();
            break;
    }
}
