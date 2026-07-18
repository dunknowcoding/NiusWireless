/*
 * pn532_i2c_basic - Minimal PN532 example over I2C.
 *
 * Detects an ISO14443A tag and prints its UID / ATQA / SAK / Type.
 * The smallest sketch that exercises the I2C transport.
 *
 * --- Wiring ---
 *   PN532 SDA  -> board SDA (D20)
 *   PN532 SCL  -> board SCL (D21)
 *   PN532 VCC  -> 3V3
 *   PN532 GND  -> GND
 *   PN532 IRQ  -> D9 (required on SAMD21)
 *   PN532 RSTO -> board RESET (or a GPIO set as PN532_RST below)
 *
 * RobotDyn SAMD21 M0-Mini: SDA=D20, SCL=D21, IRQ=D9.
 *
 * DIP: I2C mode (Elechouse: SW1=ON, SW2=OFF).
 * After changing DIP, the PN532 must see an RSTO/power-on edge so I0/I1
 * re-latch — USB reconnect or RESET button if RSTO is tied to board RESET.
 *
 * Loop: cardPresentWake() → printInfo() (UID / ATQA / SAK / Type) → halt().
 * cardPresentWake() cycles RF so cards left in HALT stay detectable.
 *
 * --- Try it ---
 *   1. Upload; open Serial Monitor at 9600.
 *   2. Expect "PN532 ready: PN532 v1.x" then UID / Type lines.
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
    for (uint16_t ms = 0; ms < 30000 && !NIUS_SERIAL; ++ms) {
        delay(1);
    }
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 I2C"));

    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 begin() failed"));
        while (1) { delay(500); }
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
}

void loop() {
    if (!nfc.cardPresentWake()) {
        delay(50);
        return;
    }
    nfc.printInfo();   // UID / ATQA / SAK / Type
    nfc.halt();
}
