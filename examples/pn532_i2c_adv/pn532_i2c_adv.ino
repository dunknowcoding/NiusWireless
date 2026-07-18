/*
 * pn532_i2c_adv - PN532 advanced example over I2C.
 *
 * Detects card family (Classic / Ultralight / other), reports detection
 * errors by NIUS_ERR_* name, then runs a safe exercise for that family:
 *
 *   MIFARE Classic 1K / 4K / Mini:
 *     - dumpToSerial() — full Key-A dump (factory key by default)
 *     - setUid(...): always dry-run (BCC + manufacturer preview).
 *       Optional commit writes the *same* UID back (CUID/magic only) so
 *       BCC/layout are verified without changing the card identity.
 *     - Data block 4: read → write marker → read-back → restore
 *     - Optional Key A change on sector 1 trailer (block 7) only — Key B
 *       and access bits are never modified; factory Key A is restored.
 *
 *   MIFARE Ultralight / NTAG:
 *     - dumpToSerial() — pages until READ fails
 *     - readPage(4) → 16 bytes; writePage(4) marker on first 4 bytes → restore
 *       (pages 0–3 refused by writePage)
 *
 * Loop uses cardPresentWake() so halt()'d cards are re-selected via RF cycle.
 *
 * --- Safety (Classic) ---
 *   writeBlock() refuses block 0 and sector trailers unless force=true.
 *   setUid() recomputes BCC = XOR(UID) and keeps manufacturer bytes.
 *   Key demo touches ONLY Sector 1 Key A (trailer block 7):
 *       Key A: FFFFFFFFFFFF  →  A1A2A3A4A5A6  (then restored)
 *       Key B: unchanged (FFFFFFFFFFFF)
 *       Access bits: unchanged (must already be transport FF 07 80 69)
 *
 * --- Wiring ---
 *   PN532 SDA / SCL / VCC / GND — same as pn532_i2c_basic
 *   PN532 IRQ — required on SAMD21 (default D9); optional elsewhere
 *
 * ESP32 custom pins: call Wire.begin(SDA, SCL) before nfc.begin().
 *
 * --- Optional compile flags ---
 *   -DPN532_ADV_COMMIT_UID=1     commit same-UID rewrite (CUID only)
 *   -DPN532_ADV_DEMO_KEY_CHANGE=1  temp Key A change on sector 1 / block 7
 */

#include <NiusWireless.h>
#include <string.h>

#if !defined(PN532_IRQ)
  #if defined(ARDUINO_ARCH_SAMD)
    #define PN532_IRQ  9
  #else
    #define PN532_IRQ  0xFF
  #endif
#endif
#ifndef PN532_RST
  #define PN532_RST  0xFF
#endif

#ifndef PN532_ADV_COMMIT_UID
  #define PN532_ADV_COMMIT_UID  0
#endif
#ifndef PN532_ADV_DEMO_KEY_CHANGE
  #define PN532_ADV_DEMO_KEY_CHANGE  0
#endif

NiusPN532 nfc(Wire, PN532_IRQ, PN532_RST);

/* Sector 1 Key A demo only (block 7). Never touch Key B or other sectors. */
static const uint8_t DEMO_KEY_A_SECTOR1[6] = {
    0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6
};

static void printHex(const uint8_t *p, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        if (i) NIUS_SERIAL.print(' ');
        if (p[i] < 0x10) NIUS_SERIAL.print('0');
        NIUS_SERIAL.print(p[i], HEX);
    }
    NIUS_SERIAL.println();
}

static void printHex16(const uint8_t *block) {
    printHex(block, 16);
}

static bool isTransportAccessBits(const uint8_t *trailer) {
    /* Bytes 6..9 of a factory-transport trailer are typically FF 07 80 69 */
    return trailer[6] == 0xFF && trailer[7] == 0x07 &&
           trailer[8] == 0x80 && trailer[9] == 0x69;
}

static void op_Classic() {
    NIUS_SERIAL.println(F("--- MIFARE Classic ---"));
    nfc.dumpToSerial();

    /*
     * UID path (card still selected after dump's final wake).
     * Dry-run always. Optional commit rewrites the *same* UID so BCC /
     * manufacturer layout are verified without changing identity.
     * Stock (non-CUID) cards fail commit — expected.
     */
    {
        uint8_t sameUid[10];
        uint8_t sameLen = nfc.uidLen;
        memcpy(sameUid, nfc.uid, sameLen);
        uint8_t su = nfc.setUid(sameUid, sameLen, PN532_ADV_COMMIT_UID != 0);
        if (su != NIUS_OK) {
            NIUS_SERIAL.print(F("setUid: "));
            NIUS_SERIAL.println(NiusPN532::errorName(su));
        }
        nfc.stopCrypto();
    }

    if (!nfc.cardPresentWake()) {
        NIUS_SERIAL.print(F("(re-detect after setUid: "));
        NIUS_SERIAL.print(NiusPN532::errorName(nfc.lastError));
        NIUS_SERIAL.println(F(")"));
        return;
    }

    if (nfc.authenticate(4, NIUS_KEY_A, nullptr) != NIUS_OK) {
        NIUS_SERIAL.print(F("(auth block 4 failed: "));
        NIUS_SERIAL.print(NiusPN532::errorName(NIUS_ERR_AUTH));
        NIUS_SERIAL.println(F(" — non-factory key?)"));
        nfc.stopCrypto();
        return;
    }

    uint8_t original[16];
    if (nfc.readBlock(4, original) != NIUS_OK) {
        NIUS_SERIAL.println(F("readBlock(4) failed"));
        nfc.stopCrypto();
        return;
    }
    NIUS_SERIAL.print(F("Block 4: "));
    printHex16(original);

    uint8_t marker[16] = {
        'N', 'i', 'u', 's', 'A', 'd', 'v', '!',
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    if (nfc.writeBlock(4, marker) == NIUS_OK) {
        uint8_t tmp[16];
        if (nfc.readBlock(4, tmp) == NIUS_OK) {
            NIUS_SERIAL.print(F("read-back: "));
            printHex16(tmp);
        }
        nfc.writeBlock(4, original);
        NIUS_SERIAL.println(F("(original block 4 restored)"));
    } else {
        NIUS_SERIAL.println(F("writeBlock(4) failed (data block only)"));
    }
    nfc.stopCrypto();

#if PN532_ADV_DEMO_KEY_CHANGE
    /*
     * Key change (sector 1 / trailer block 7) — Key A ONLY:
     *   before: Key A = FFFFFFFFFFFF, Key B = FFFFFFFFFFFF, AC = FF 07 80 69
     *   during: Key A = A1A2A3A4A5A6, Key B unchanged, AC unchanged
     *   after:  Key A restored to FFFFFFFFFFFF
     * Skipped unless trailer already has transport access bits.
     */
    if (nfc.cardPresentWake() &&
        nfc.authenticate(7, NIUS_KEY_A, nullptr) == NIUS_OK) {
        uint8_t trailer[16];
        if (nfc.readBlock(7, trailer) == NIUS_OK &&
            isTransportAccessBits(trailer)) {
            uint8_t modified[16];
            memcpy(modified, trailer, 16);
            memcpy(modified, DEMO_KEY_A_SECTOR1, 6); /* Key A only */

            NIUS_SERIAL.println(F("Key demo: sector 1 trailer block 7"));
            NIUS_SERIAL.println(F("  Key A -> A1A2A3A4A5A6 (Key B + AC kept)"));
            if (nfc.writeBlock(7, modified, true) == NIUS_OK) {
                nfc.stopCrypto();
                if (nfc.cardPresentWake() &&
                    nfc.authenticate(7, NIUS_KEY_A,
                                     (uint8_t *)DEMO_KEY_A_SECTOR1) == NIUS_OK) {
                    NIUS_SERIAL.println(F("  auth with demo Key A OK"));
                    nfc.writeBlock(7, trailer, true); /* restore factory Key A */
                    NIUS_SERIAL.println(F("  Key A restored to FFFFFFFFFFFF"));
                } else {
                    NIUS_SERIAL.println(F("  ERROR: demo Key A auth failed — trying restore"));
                    if (nfc.cardPresentWake() &&
                        nfc.authenticate(7, NIUS_KEY_A, nullptr) == NIUS_OK) {
                        nfc.writeBlock(7, trailer, true);
                    }
                }
            }
        } else {
            NIUS_SERIAL.println(F("(key demo skipped — unexpected access bits)"));
        }
        nfc.stopCrypto();
    }
#else
    NIUS_SERIAL.println(F("(key demo off — define PN532_ADV_DEMO_KEY_CHANGE=1)"));
#endif
}

static void op_Ultralight() {
    NIUS_SERIAL.println(F("--- MIFARE Ultralight / NTAG ---"));
    nfc.dumpToSerial();

    /* readPage returns 16 bytes (4 pages); writePage writes one 4-byte page. */
    uint8_t page4[16];
    if (nfc.readPage(4, page4) != NIUS_OK) {
        NIUS_SERIAL.println(F("readPage(4) failed"));
        return;
    }
    NIUS_SERIAL.print(F("Page 4: "));
    printHex(page4, 4);

    uint8_t marker[4] = { 'N', 'I', '!', 0x00 };
    if (nfc.writePage(4, marker) == NIUS_OK) {
        uint8_t tmp[16];
        if (nfc.readPage(4, tmp) == NIUS_OK) {
            NIUS_SERIAL.print(F("read-back: "));
            printHex(tmp, 4);
        }
        nfc.writePage(4, page4); /* restore first 4 bytes only */
        NIUS_SERIAL.println(F("(original page 4 restored)"));
    } else {
        NIUS_SERIAL.println(F("writePage(4) failed"));
    }
}

static void op_Unsupported() {
    NIUS_SERIAL.print(F("(unsupported family: "));
    NIUS_SERIAL.print(nfc.getCardTypeName());
    NIUS_SERIAL.println(F(")"));
}

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 30000 && !NIUS_SERIAL; ++ms) delay(1);
#endif
#if defined(ARDUINO_ARCH_ESP32)
    // Wire.begin(21, 22);   // uncomment: your ESP32 SDA / SCL pins
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 I2C (advanced)"));

    nfc.setI2CClock(100000UL);
    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 not found at 100 kHz, retry @ 400 kHz..."));
        nfc.setI2CClock(400000UL);
        if (!nfc.begin()) {
            NIUS_SERIAL.println(F("ERROR: PN532 begin() failed"));
            while (1) { delay(500); }
        }
    } else {
        /* Faster host bus after a solid bring-up. */
        nfc.setI2CClock(400000UL);
    }

    uint32_t ver = 0;
    if (nfc.getFirmwareVersion(ver)) {
        NIUS_SERIAL.print(F("IC=0x"));
        NIUS_SERIAL.print((uint8_t)(ver >> 24), HEX);
        NIUS_SERIAL.print(F("  FW="));
        NIUS_SERIAL.print((uint8_t)(ver >> 16));
        NIUS_SERIAL.print('.');
        NIUS_SERIAL.print((uint8_t)(ver >> 8));
        NIUS_SERIAL.print(F("  support=0x"));
        NIUS_SERIAL.println((uint8_t)ver, HEX);
    }

    /* Low MxRty → fast empty-field polls; still reliable with card present. */
    if (!nfc.setPassiveActivationRetries(0x02)) {
        NIUS_SERIAL.println(F("(setPassiveActivationRetries failed — continuing)"));
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresentWake()) {
        /* Occasional idle diagnostic (not every poll). */
        static uint16_t misses;
        if ((++misses % 40) == 0) {
            NIUS_SERIAL.print(F("detect: "));
            NIUS_SERIAL.println(NiusPN532::errorName(nfc.lastError));
        }
        delay(20);
        return;
    }

    nfc.printInfo();

    switch (nfc.getCardType()) {
        case NIUS_CARD_MIFARE_1K:
        case NIUS_CARD_MIFARE_4K:
        case NIUS_CARD_MIFARE_MINI:
            op_Classic();
            break;
        case NIUS_CARD_MIFARE_UL:
            op_Ultralight();
            break;
        default:
            op_Unsupported();
            break;
    }

    nfc.halt();
    NIUS_SERIAL.println();
    delay(500);
}
