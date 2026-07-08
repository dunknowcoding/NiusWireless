/*
 * rc522_tag — Type-adaptive tag operations
 *
 * Modelled on the standard MFRC522 library's DumpInfo / ChangeUID sketches
 * (https://github.com/miguelbalboa/rfid). One entry point: detect the
 * tag, then dispatch to the right flow for its type.
 *
 *   MIFARE Classic 1K / 4K / Mini (S50 / keychain / fob / CUID / Gen2):
 *     - dumpClassic() reads every block the default key can authenticate
 *     - setUid() changes the UID on CUID / DirectWrite / FUID cards
 *
 *   MIFARE Ultralight / NTAG / NTAG213/215/216:
 *     - dumpUltralight() reads all pages that respond
 *     - readPage() / writePage() for individual page access
 *
 *   ISO 14443-4 / DESFire / Plus / ISO 18092 / unknown:
 *     - prints the detected type and recommends a different tool
 *
 * All three flows use the high-level NiusRC522 methods — no manual
 * authenticate / readBlock / writeBlock loops.
 *
 * If you know a custom key for a MIFARE Classic tag, type the 12 hex
 * digits on Serial during setup() + Enter (5 s timeout). The sketch will
 * use it instead of the factory FFFFFFFFFFFF.
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

// --- Pin definitions ---
#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12

// Factory key for MIFARE Classic.
static const uint8_t DEFAULT_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Optional user-supplied key (entered in setup()).
static uint8_t userKey[6];
static bool    useUserKey = false;

NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void printLine() {
    Serial.println(F("---------------------------------------------------------------"));
}

// Print one 16-byte block in the standard MFRC522 DumpInfo format:
//   "  Block  X:  HH HH HH HH HH HH HH HH  HH HH HH HH HH HH HH HH  [................]"
static void printBlock(uint8_t block, uint8_t *data) {
    Serial.print(F("    Block "));
    if (block < 100) Serial.print(' ');
    if (block < 10)  Serial.print(' ');
    Serial.print(block);
    Serial.print(F(":  "));
    for (uint8_t i = 0; i < 16; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.print(F(" ["));
    for (uint8_t i = 0; i < 16; i++) {
        char c = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : ' ';
        Serial.print(c);
    }
    Serial.println(']');
}

// Adapter that the new dumpClassic() callback expects: receives a 16-byte
// chunk with no block index, so we just print a 4-line group. This is
// called once per block the library reads successfully. Uses a global
// counter (see resetChunkCounter) so block numbers reset to 0 at the
// start of each tag dump.
static uint16_t chunkStart = 0;
static void printChunk(uint8_t *data) {
    for (uint8_t i = 0; i < 4; i++) {
        printBlock((uint8_t)(chunkStart + i), data + i * 4);
    }
    chunkStart += 4;
}

static void resetChunkCounter() {
    chunkStart = 0;
}

static void promptUserKey() {
    Serial.println();
    Serial.println(F("If you know the 12-hex-digit key for this tag, type it now"));
    Serial.println(F("(e.g. AABBCCDDEEFF) and press Enter. Press Enter alone to use"));
    Serial.println(F("the built-in default key (FFFFFFFFFFFF). Waiting 5s..."));

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
        Serial.println(F("No key typed — using factory key."));
        return;
    }
    for (uint8_t i = 0; i < 6; i++) {
        char hex[3] = {input[i*2], input[i*2+1], 0};
        char *endp;
        long v = strtol(hex, &endp, 16);
        if (*endp != 0 || v < 0 || v > 0xFF) {
            Serial.println(F("Invalid hex — using factory key."));
            return;
        }
        userKey[i] = (uint8_t)v;
    }
    useUserKey = true;
    Serial.print(F("Using user key: "));
    for (uint8_t i = 0; i < 6; i++) {
        if (userKey[i] < 0x10) Serial.print('0');
        Serial.print(userKey[i], HEX);
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// Per-type flows
// ---------------------------------------------------------------------------

// MIFARE Classic 1K / 4K / Mini — single high-level dump call.
static void runClassicFlow() {
    Serial.println();
    printLine();
    Serial.println(F("Dump with default key (FFFFFFFFFFFF)"));
    printLine();

    const uint8_t *key = useUserKey ? userKey : DEFAULT_KEY;
    Serial.print(F("  Using key: "));
    for (uint8_t i = 0; i < 6; i++) {
        if (key[i] < 0x10) Serial.print('0');
        Serial.print(key[i], HEX);
    }
    Serial.println();

    resetChunkCounter();
    uint8_t okSectors = rfid.dumpClassic((uint8_t *)key, printChunk);
    Serial.println();
    Serial.print(F("  Authenticated sectors: "));
    Serial.print(okSectors);
    Serial.println(F(" / 16"));
    if (okSectors < 16) {
        Serial.println(F("  Some sectors used a different key."));
        Serial.println(F("  Either edit tryKeys[] in the source, type the key"));
        Serial.println(F("  on Serial during setup(), or use a tool like Proxmark3"));
        Serial.println(F("  to recover the keys."));
    }

    // CUID UID-change demo — only meaningful on Chinese "magic" cards.
    Serial.println();
    printLine();
    Serial.println(F("CUID UID-change demo (works on Chinese magic cards)"));
    printLine();

    if (rfid.uidLen == 4) {
        uint8_t newUid[4] = {rfid.uid[0], rfid.uid[1], rfid.uid[2],
                             (uint8_t)(rfid.uid[3] + 1)};
        Serial.print(F("  Current UID: "));
        for (uint8_t i = 0; i < 4; i++) {
            if (rfid.uid[i] < 0x10) Serial.print('0');
            Serial.print(rfid.uid[i], HEX);
        }
        Serial.println();
        Serial.print(F("  New UID:     "));
        for (uint8_t i = 0; i < 4; i++) {
            if (newUid[i] < 0x10) Serial.print('0');
            Serial.print(newUid[i], HEX);
        }
        Serial.println();
        Serial.println(F("  (Standard MIFARE Classic rejects this — non-CUID cards"));
        Serial.println(F("   will keep their original UID. Only Chinese CUID /"));
        Serial.println(F("   DirectWrite / FUID cards will accept the change.)"));

        uint8_t r = rfid.setUid(newUid, 4);
        Serial.print(F("  setUid() result: "));
        Serial.println(r == NIUS_OK ? F("OK") : F("FAILED (not a magic card)"));
    } else {
        Serial.println(F("  UID is not 4 bytes — skipping."));
    }
}

// MIFARE Ultralight / NTAG — single high-level dump call.
static void runUltralightFlow() {
    Serial.println();
    printLine();
    Serial.println(F("Dump all pages"));
    printLine();

    resetChunkCounter();
    uint8_t pages = rfid.dumpUltralight(printChunk);
    Serial.println();
    Serial.print(F("  Dumped "));
    Serial.print(pages);
    Serial.println(F(" pages (4 bytes each)."));

    // Try to get the version too — only Ultralight EV1 / NTAG reply.
    uint8_t v[8];
    if (rfid.getNTAGVersion(v) == NIUS_OK) {
        Serial.print(F("  GET_VERSION: "));
        Serial.print(v[1], HEX);
        Serial.print('.');
        Serial.print(v[2], HEX);
        Serial.print('.');
        Serial.print(v[3], HEX);
        Serial.println();
    }
}

static void runUnsupportedFlow() {
    Serial.println();
    printLine();
    Serial.println(F("Unsupported type — NiusWireless does not operate on this family"));
    printLine();
    switch (rfid.getCardType()) {
        case NIUS_CARD_ISO14443_4:
        case NIUS_CARD_DESFIRE:
            Serial.println(F("  ISO 14443-4 / DESFire detected. Use PN532 or PN532-based"));
            Serial.println(F("  tools (libnfc, mfcuk, MCT on Android) for APDU-level ops."));
            break;
        case NIUS_CARD_ISO18092:
            Serial.println(F("  ISO 18092 (NFC-IP1) peer — not a readable tag."));
            break;
        case NIUS_CARD_MIFARE_PLUS:
            Serial.println(F("  MIFARE Plus detected. SL1 (Classic emulation) may work;"));
            Serial.println(F("  SL2/SL3 need AES — not implemented in NiusWireless."));
            break;
        default:
            Serial.println(F("  No NiusWireless flow matches this type."));
            break;
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println(F("NiusWireless — RC522 Tag Operations (DumpInfo / ChangeUID style)"));
    Serial.println(F("=================================================================="));
    Serial.println(F("Modelled on the MFRC522 library examples:"));
    Serial.println(F("  https://github.com/miguelbalboa/rfid"));
    Serial.println();

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
// Loop
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

    // Halt the tag and wait for it to leave the field.
    rfid.halt();
    uint32_t waitStart = millis();
    while ((millis() - waitStart) < 5000UL) {
        if (!rfid.cardPresentWake()) { break; }
        rfid.halt();
        delay(150);
    }
}
