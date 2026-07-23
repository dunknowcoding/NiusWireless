/*
 * pn532_spi_scan - Step 1: confirm PN532 on SPI before pn532_spi_basic.
 *
 * Uses NiusPN532::begin() (GetFirmwareVersion + SAMConfiguration) so the
 * host SPI path is exercised without a generic status-byte probe.
 *
 * After begin(), the loop optionally prints UID / ATQA / SAK / Type via
 * printInfo(), then halt(); loop prefers cardPresent() then cardPresentWake().
 *
 * --- Wiring ---
 *   PN532 SCK/MOSI/MISO / SS / VCC / GND — same as pn532_spi_basic
 *   IRQ optional (unwired here; status-byte ready)
 *
 * DIP: Elechouse SPI — SW1=OFF, SW2=ON.
 * To switch I2C / SPI / HSU: cut power first, set the DIP, then repower
 * so I0/I1 re-latch (USB unplug/replug, or RESET if RSTO → board RESET).
 *
 * --- Try it ---
 *   1. Upload; open Serial Monitor @ 9600.
 *   2. Expect "SPI device FOUND: PN532 v1.x".
 *   3. Then upload pn532_spi_basic or pn532_spi_adv.
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
    for (uint16_t ms = 0; ms < 8000 && !NIUS_SERIAL; ++ms) delay(1);
#endif

    NIUS_SERIAL.println(F("=== PN532 SPI check (ICSP / SS=D8) ==="));
    NIUS_SERIAL.println(F("Probing..."));
    NIUS_SERIAL.flush();

    nfc.setSpiClock(2000000UL);
    const bool ok = nfc.begin();
    if (ok) {
        NIUS_SERIAL.print(F("SPI device FOUND: "));
        NIUS_SERIAL.println(nfc.getVersion());
        NIUS_SERIAL.println(F("OK — upload pn532_spi_basic next"));
    } else {
        NIUS_SERIAL.println(F("SPI device NOT FOUND"));
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
