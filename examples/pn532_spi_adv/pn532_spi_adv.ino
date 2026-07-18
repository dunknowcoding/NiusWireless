/*
 * pn532_spi_adv - PN532 advanced example over SPI with IRQ.
 *
 * Uses IRQ (D9) for ready signalling, prints firmware info, sets retries,
 * then Classic block 4 read / write / restore.
 *
 * --- Wiring (RobotDyn SAMD21 M0-Mini — see SAMD21-M0-Mini.pdf) ---
 *   PN532 SCK/MOSI/MISO -> board ICSP
 *   PN532 SS            -> D8
 *   PN532 IRQ           -> D9
 *   PN532 RSTO          -> board RESET
 *   PN532 VCC / GND     -> 3V3 / GND
 *
 * DIP: Elechouse SPI — SW1=OFF, SW2=ON.
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

NiusPN532 nfc(PN532_CS, PN532_RST, true);

static const uint8_t FACTORY_KEY[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static void printHex16(const uint8_t *block) {
    for (uint8_t i = 0; i < 16; i++) {
        if (i) NIUS_SERIAL.print(' ');
        if (block[i] < 0x10) NIUS_SERIAL.print('0');
        NIUS_SERIAL.print(block[i], HEX);
    }
    NIUS_SERIAL.println();
}

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 30000 && !NIUS_SERIAL; ++ms) {
        delay(1);
    }
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 SPI (advanced)"));

    nfc.setIRQPin(PN532_IRQ);

    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 begin() failed"));
        while (1) { delay(500); }
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

    if (!nfc.setPassiveActivationRetries(0x02)) {
        NIUS_SERIAL.println(F("(setPassiveActivationRetries failed — continuing)"));
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresent()) {
        delay(50);
        return;
    }

    nfc.printInfo();

    if (nfc.authenticate(4, NIUS_KEY_A, (uint8_t *)FACTORY_KEY) != NIUS_OK) {
        NIUS_SERIAL.println(F("(auth failed — not Classic, or non-factory key)"));
        NIUS_SERIAL.println();
        delay(800);
        return;
    }

    uint8_t original[16];
    if (nfc.readBlock(4, original) != NIUS_OK) {
        NIUS_SERIAL.println(F("readBlock(4) failed"));
        NIUS_SERIAL.println();
        delay(800);
        return;
    }
    NIUS_SERIAL.print(F("Block 4: "));
    printHex16(original);

    uint8_t marker[16] = {
        'N', 'i', 'u', 's', 'A', 'd', 'v', '!',
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    if (nfc.writeBlock(4, marker) == NIUS_OK &&
        nfc.cardPresent() &&
        nfc.authenticate(4, NIUS_KEY_A, (uint8_t *)FACTORY_KEY) == NIUS_OK) {
        uint8_t tmp[16];
        if (nfc.readBlock(4, tmp) == NIUS_OK) {
            NIUS_SERIAL.print(F("read-back: "));
            printHex16(tmp);
        }
        nfc.writeBlock(4, original);
        NIUS_SERIAL.println(F("(original block 4 restored)"));
    } else {
        NIUS_SERIAL.println(F("write / verify failed"));
    }

    NIUS_SERIAL.println();
    delay(800);
}
