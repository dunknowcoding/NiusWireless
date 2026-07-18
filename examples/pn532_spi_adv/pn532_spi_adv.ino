/*
 * pn532_spi_adv - PN532 advanced example over SPI with IRQ.
 *
 * Same card-family flow as pn532_i2c_adv: type name, detection errors,
 * Classic block-4 roundtrip, setUid dry-run, optional Key A demo on
 * sector 1 trailer (block 7) only — see that sketch for safety notes.
 *
 * --- Wiring (RobotDyn SAMD21 M0-Mini — see SAMD21-M0-Mini.pdf) ---
 *   PN532 SCK/MOSI/MISO -> board ICSP
 *   PN532 SS            -> D8
 *   PN532 IRQ           -> D9
 *   PN532 RSTO          -> board RESET
 *   PN532 VCC / GND     -> 3V3 / GND
 *
 * DIP: Elechouse SPI — SW1=OFF, SW2=ON.
 *
 * Optional: -DPN532_ADV_COMMIT_UID=1  -DPN532_ADV_DEMO_KEY_CHANGE=1
 */

#include <NiusWireless.h>
#include <string.h>

#ifndef PN532_CS
  #define PN532_CS   8
#endif
#if !defined(PN532_IRQ)
  #if defined(ARDUINO_ARCH_SAMD)
    #define PN532_IRQ  9
  #else
    #define PN532_IRQ  2
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

NiusPN532 nfc(PN532_CS, PN532_RST, true);

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

static bool isTransportAccessBits(const uint8_t *trailer) {
    return trailer[6] == 0xFF && trailer[7] == 0x07 &&
           trailer[8] == 0x80 && trailer[9] == 0x69;
}

static void op_Classic() {
    NIUS_SERIAL.println(F("--- MIFARE Classic ---"));

    /* setUid dry-run first — see pn532_i2c_adv for BCC / commit notes. */
    {
        uint8_t sameUid[10];
        uint8_t sameLen = nfc.uidLen;
        memcpy(sameUid, nfc.uid, sameLen);
        (void)nfc.setUid(sameUid, sameLen, PN532_ADV_COMMIT_UID != 0);
        nfc.stopCrypto();
    }
    if (!nfc.cardPresent()) {
        return;
    }

    if (nfc.authenticate(4, NIUS_KEY_A, nullptr) != NIUS_OK) {
        NIUS_SERIAL.println(F("(auth block 4 failed)"));
        nfc.stopCrypto();
        return;
    }

    uint8_t original[16];
    if (nfc.readBlock(4, original) != NIUS_OK) {
        nfc.stopCrypto();
        return;
    }
    NIUS_SERIAL.print(F("Block 4: "));
    printHex(original, 16);

    uint8_t marker[16] = {
        'N', 'i', 'u', 's', 'S', 'P', 'I', '!',
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    if (nfc.writeBlock(4, marker) == NIUS_OK) {
        uint8_t tmp[16];
        if (nfc.readBlock(4, tmp) == NIUS_OK) {
            NIUS_SERIAL.print(F("read-back: "));
            printHex(tmp, 16);
        }
        nfc.writeBlock(4, original);
        NIUS_SERIAL.println(F("(original block 4 restored)"));
    }
    nfc.stopCrypto();

#if PN532_ADV_DEMO_KEY_CHANGE
    /* Sector 1 / block 7 Key A only — see pn532_i2c_adv comments. */
    if (nfc.cardPresent() &&
        nfc.authenticate(7, NIUS_KEY_A, nullptr) == NIUS_OK) {
        uint8_t trailer[16];
        if (nfc.readBlock(7, trailer) == NIUS_OK &&
            isTransportAccessBits(trailer)) {
            uint8_t modified[16];
            memcpy(modified, trailer, 16);
            memcpy(modified, DEMO_KEY_A_SECTOR1, 6);
            if (nfc.writeBlock(7, modified, true) == NIUS_OK) {
                nfc.stopCrypto();
                if (nfc.cardPresent() &&
                    nfc.authenticate(7, NIUS_KEY_A,
                                     (uint8_t *)DEMO_KEY_A_SECTOR1) == NIUS_OK) {
                    nfc.writeBlock(7, trailer, true);
                    NIUS_SERIAL.println(F("  Key A restored to FFFFFFFFFFFF"));
                }
            }
        }
        nfc.stopCrypto();
    }
#endif
}

static void op_Ultralight() {
    NIUS_SERIAL.println(F("--- MIFARE Ultralight / NTAG ---"));
    uint8_t page4[4];
    if (nfc.readPage(4, page4) != NIUS_OK) return;
    uint8_t marker[4] = { 'N', 'S', '!', 0x00 };
    if (nfc.writePage(4, marker) == NIUS_OK) {
        nfc.writePage(4, page4);
        NIUS_SERIAL.println(F("(original page 4 restored)"));
    }
}

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 30000 && !NIUS_SERIAL; ++ms) delay(1);
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 SPI (advanced)"));

    nfc.setIRQPin(PN532_IRQ);
    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 begin() failed"));
        while (1) { delay(500); }
    }
    (void)nfc.setPassiveActivationRetries(0x02);

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresent()) {
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
            NIUS_SERIAL.println(F("(unsupported family)"));
            break;
    }
    nfc.halt();
    NIUS_SERIAL.println();
    delay(500);
}
