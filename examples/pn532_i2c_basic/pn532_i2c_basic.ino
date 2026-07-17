/*
 * pn532_i2c_basic - Minimal PN532 example over I2C.
 *
 * Detects an ISO14443A tag and prints its UID / ATQA / SAK. The smallest
 * sketch that exercises the I2C transport - about 40 lines.
 *
 * I2C on the PN532 is a 4-wire bus only (no IRQ / RSTO). Those pins are
 * used in SPI mode — see the pn532_spi_* examples later.
 *
 * --- Wiring (RobotDyn SAMD21 M0-Mini / Arduino Zero) ---
 *   PN532 SDA  -> D20 / SDA
 *   PN532 SCL  -> D21 / SCL
 *   PN532 VCC  -> 3V3
 *   PN532 GND  -> GND
 *
 * On the PN532 breakout, set DIP switches / jumpers to I2C mode
 * (typically SEL0=LOW, SEL1=HIGH — check your module's silk).
 *
 * --- Try it ---
 *   1. Place a card on the reader.
 *   2. Open Serial Monitor at 9600 baud.
 *   3. Watch UID / ATQA / SAK get printed.
 *
 * SAMD M0-Mini / Zero (Native USB): examples use NIUS_SERIAL (SerialUSB).
 * A short USB-CDC wait in setup lets the host open the port before boot logs.
 */

#include <NiusWireless.h>

/* irqPin / rstPin unused on 4-wire I2C — pass 0xFF to poll over the bus. */
NiusPN532 nfc(0xFF, 0xFF);

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 3000 && !NIUS_SERIAL; ++ms) {
        delay(1);
    }
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 I2C"));

    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 not found. Check wiring / I2C mode."));
        while (1) { delay(500); }
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresent()) return;
    nfc.printInfo();                          // UID / ATQA / SAK
    delay(1000);                              // debounce
}
