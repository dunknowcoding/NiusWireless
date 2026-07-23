/*
 * pn532_i2c_scan - Step 1: confirm PN532 on I2C before pn532_i2c_basic.
 *
 * Do not use a generic I2C scanner on 0x24 — PN532 clock-stretches and can
 * hang SAMD21 Wire. This sketch uses NiusPN532::begin() instead.
 *
 * After begin(), the loop prints UID / ATQA / SAK / Type via printInfo(),
 * then halt(); loop prefers cardPresent() then cardPresentWake().
 *
 * --- Wiring ---
 *   PN532 SDA / SCL / VCC / GND — same as pn532_i2c_basic
 *   PN532 IRQ — required on SAMD21 (default D9); optional elsewhere
 *
 *
 * DIP: I2C mode (Elechouse: SW1=ON, SW2=OFF).
 * To switch I2C / SPI / HSU: cut power first, set the DIP, then repower
 * so I0/I1 re-latch (USB unplug/replug, or RESET if RSTO → board RESET).
 * --- Try it ---
 *   1. Upload; open Serial Monitor @ 9600.
 *   2. Expect "I2C device FOUND: PN532 v1.x".
 *   3. Then upload pn532_i2c_basic or pn532_i2c_adv.
 */

#include <NiusWireless.h>

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

NiusPN532 nfc(PN532_IRQ, PN532_RST);

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 8000 && !NIUS_SERIAL; ++ms) delay(1);
#endif

    NIUS_SERIAL.println(F("=== PN532 I2C check @ 0x24 ==="));
    NIUS_SERIAL.println(F("Probing..."));
    NIUS_SERIAL.flush();
    const bool ok = nfc.begin();
    if (ok) {
        NIUS_SERIAL.print(F("I2C device FOUND: "));
        NIUS_SERIAL.println(nfc.getVersion());
        NIUS_SERIAL.println(F("OK — upload pn532_i2c_basic next"));
    } else {
        NIUS_SERIAL.println(F("I2C device NOT FOUND"));
        while (1) delay(1000);
    }
}

void loop() {
    if (!nfc.cardPresent() && !nfc.cardPresentWake()) {
        delay(100);
        return;
    }
    nfc.printInfo();   // UID / ATQA / SAK / Type
    nfc.halt();
    delay(500);
}
