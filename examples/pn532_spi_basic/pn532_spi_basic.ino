/*
 * pn532_spi_basic - Minimal PN532 example over SPI.
 *
 * Detects an ISO14443A tag and prints its UID / ATQA / SAK / Type.
 * Uses status-byte ready (no IRQ handler required).
 * After printInfo(), halt() + cardPresentWake() keep HALT'd cards visible.
 *
 * --- Wiring (RobotDyn SAMD21 M0-Mini — SAMD21-M0-Mini.pdf) ---
 *   PN532 SCK/MOSI/MISO -> ICSP
 *   PN532 SS            -> D8
 *   PN532 IRQ           -> D9 (optional)
 *   PN532 RSTO          -> board RESET
 *   PN532 VCC / GND     -> 3V3 / GND
 *
 * DIP: SW1=OFF, SW2=ON (SPI).
 *
 * --- Try it ---
 *   1. Upload; open Serial Monitor at 9600.
 *   2. Expect "PN532 ready: PN532 v1.x" then UID / Type lines.
 */

#include <NiusWireless.h>

#ifndef PN532_CS
  #define PN532_CS   8
#endif
#ifndef PN532_RST
  #define PN532_RST  0xFF
#endif

NiusPN532 nfc(PN532_CS, PN532_RST, true);

void setup() {
    NIUS_SERIAL.begin(9600);
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
    for (uint16_t ms = 0; ms < 30000 && !NIUS_SERIAL; ++ms) {
        delay(1);
    }
#endif
    NIUS_SERIAL.println(F("NiusWireless PN532 SPI"));

    if (!nfc.begin()) {
        NIUS_SERIAL.println(F("ERROR: PN532 begin() failed"));
        while (1) { delay(500); }
    }

    NIUS_SERIAL.print(F("PN532 ready: "));
    NIUS_SERIAL.println(nfc.getVersion());
    NIUS_SERIAL.println(F("Hold an ISO14443A tag near the reader..."));
    delay(500);
}

void loop() {
    if (!nfc.cardPresentWake()) {
        delay(200);
        return;
    }
    nfc.printInfo();   // UID / ATQA / SAK / Type
    nfc.halt();
    delay(500);
}
