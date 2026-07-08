/*
 * rc522_recover — Proxmark3-style key recovery for MIFARE Classic
 *
 * Modelled on the Proxmark3 workflow:
 *   1. Dictionary attack — try a list of well-known default keys
 *      against every sector. Fast, often enough on factory-fresh or
 *      vendor-default cards (S50, NTAG, Gen1/Gen2 magic cards with
 *      the default key still set on most sectors).
 *   2. Nested attack — exploit the MIFARE Classic Crypto1 cipher to
 *      recover a sector's key using *one* known key on the same card.
 *      Slower (a few minutes per sector on UNO R4) but recovers keys
 *      that are not in any public dictionary. Needs a port of the
 *      Crypto1 nested-attack code (Proxmark3 `mifarehost.c` /
 *      `crypto1.c`, or mfoc's static_nested.c).
 *   3. Darkside attack — recover a key from a single failed auth,
 *      no prior known key required. Slowest (~2800 attempts on
 *      average, several minutes per sector). Needs a port of the
 *      Crypto1 darkside code (Proxmark3 `mf_darkside.c`).
 *
 * This sketch runs phase 1 in full. Phases 2 and 3 are exposed in the
 * library API but are stubbed out — see NiusRC522.cpp for the
 * detailed TODO. A user who ports the Proxmark3 crypto1 attack into
 * a `.cpp` can wire it into the `nestedAttack` / `darksideAttack`
 * methods and use the high-level `recoverAllKeys` orchestrator
 * unchanged.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)
 *   RC522 SCK  -> SCL  (D19 / A5)
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 IRQ  -> D13
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 */

#include <NiusWireless.h>

#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12

NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

// Dictionary of well-known MIFARE Classic keys (the same set the library
// example sketches use; add your own if you know the card's vendor key).
static const uint8_t dictionary[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // NXP factory default
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // all-zero (some S50 sectors)
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},  // NXP MAD key A
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},  // NXP MAD key B
    {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5},
    {0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},  // NXP public transport
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
static const uint8_t dictionaryLen = sizeof(dictionary) / sizeof(dictionary[0]);

static void printLine() {
    Serial.println(F("---------------------------------------------------------------"));
}

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println(F("NiusWireless — RC522 Key Recovery (Proxmark3-style)"));
    Serial.println(F("====================================================="));

    if (!rfid.begin()) {
        Serial.println(F("ERROR: RC522 not found. Check wiring and power."));
        while (1) { delay(500); }
    }
    Serial.print(F("Reader ready: "));
    Serial.println(rfid.getVersion());
    Serial.println(F("Place a MIFARE Classic card on the reader..."));
    Serial.println();
}

void loop() {
    if (!rfid.cardPresentWake()) { return; }

    Serial.println();
    printLine();
    Serial.println(F("Tag detected"));
    printLine();
    Serial.print(F("  UID:  ")); Serial.println(rfid.getUID());
    Serial.print(F("  Type: ")); Serial.println(rfid.getCardTypeName());

    if (rfid.getCardType() != NIUS_CARD_MIFARE_1K &&
        rfid.getCardType() != NIUS_CARD_MIFARE_4K &&
        rfid.getCardType() != NIUS_CARD_MIFARE_MINI) {
        Serial.println(F("  Key recovery only works on MIFARE Classic. Skip."));
        rfid.halt();
        delay(1000);
        return;
    }

    // Use sector 0 as the foothold. Default FFFFFFFFFFFF is always tried
    // first; if it doesn't work, no recovery is possible without first
    // getting at least one sector's key by other means.
    const uint8_t foothold = 0;
    const uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    Serial.println();
    printLine();
    Serial.println(F("Phase 1: dictionary attack"));
    printLine();

    uint8_t recovered[16][6];
    uint16_t mask = 0;
    uint8_t count = rfid.recoverAllKeys(foothold, defaultKey,
                                        dictionary, dictionaryLen,
                                        recovered, &mask,
                                        0 /* no nested */);

    Serial.print(F("Recovered "));
    Serial.print(count);
    Serial.println(F(" / 16 keys"));
    Serial.println();
    for (uint8_t s = 0; s < 16; s++) {
        Serial.print(F("  Sector "));
        if (s < 10) Serial.print(' ');
        Serial.print(s);
        Serial.print(F(": "));
        if (mask & (1U << s)) {
            for (uint8_t i = 0; i < 6; i++) {
                if (recovered[s][i] < 0x10) Serial.print('0');
                Serial.print(recovered[s][i], HEX);
            }
            Serial.print(F("  Key"));
            Serial.println(s == foothold ? F("  (foothold)") : F(""));
        } else {
            Serial.println(F("not recovered"));
        }
    }

    Serial.println();
    printLine();
    Serial.println(F("Phase 2: nested attack"));
    printLine();
    Serial.println(F("Not yet implemented in the public release — the library"));
    Serial.println(F("exposes nestedAttack() / darksideAttack() but the full"));
    Serial.println(F("Crypto1 bit-twiddling attack code (~500 lines) needs to be"));
    Serial.println(F("ported from Proxmark3 / mfoc to use it. See the header for"));
    Serial.println(F("the API and NiusRC522.cpp for the TODO."));

    rfid.halt();
    uint32_t ws = millis();
    while ((millis() - ws) < 5000UL) {
        if (!rfid.cardPresentWake()) { break; }
        rfid.halt();
        delay(150);
    }
}
