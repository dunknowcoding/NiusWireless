/*
 * pn532_i2c_adv - PN532 advanced example over I2C.
 *
 * Exercises I2C-mode settings and Classic read / write:
 *   - explicit TwoWire bus + setI2CClock(100 kHz / 400 kHz)
 *   - GetFirmwareVersion raw fields
 *   - setPassiveActivationRetries()
 *   - printInfo() (UID / ATQA / SAK)
 *   - authenticate + readBlock / writeBlock on sector 1 (block 4)
 *
 * I2C on the PN532 is still 4-wire (SDA / SCL / VCC / GND). IRQ and RSTO
 * belong to SPI mode — interrupt-driven examples live in pn532_spi_adv.
 *
 * --- Wiring (RobotDyn SAMD21 M0-Mini / Arduino Zero) ---
 *   PN532 SDA  -> D20 / SDA
 *   PN532 SCL  -> D21 / SCL
 *   PN532 VCC  -> 3V3
 *   PN532 GND  -> GND
 *
 *   Set the module jumpers / DIP switches to I2C mode.
 */

#include <NiusWireless.h>
#include <string.h>

/* 4-wire I2C — no IRQ / RSTO */
NiusPN532 nfc(Wire, 0xFF, 0xFF);

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
    for (uint16_t ms = 0; ms < 3000 && !NIUS_SERIAL; ++ms) {
        delay(1);
    }
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 I2C (advanced)"));

    nfc.setI2CClock(100000UL);                // 100 kHz — most reliable default
    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 not found at 100 kHz, retry @ 400 kHz..."));
        nfc.setI2CClock(400000UL);
        if (!nfc.begin()) {
            NIUS_SERIAL.println(F("ERROR: PN532 not found. Check wiring / I2C mode."));
            while (1) { delay(500); }
        }
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

    /* Cap passive-activation retries so an empty field returns quickly. */
    if (!nfc.setPassiveActivationRetries(0x02)) {
        NIUS_SERIAL.println(F("(setPassiveActivationRetries failed — continuing)"));
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresent()) return;

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
