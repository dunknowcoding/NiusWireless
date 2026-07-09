/*
 * rc522_basic — Minimal RC522 example
 *
 * Just detects a tag on the reader and prints its UID / type to the
 * serial monitor. The smallest possible sketch that exercises the
 * library — about 30 lines.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA   (D18 / A4)   software-SPI chip-select
 *   RC522 SCK  -> SCL   (D19 / A5)   software-SPI clock
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V   (do not power from 5V)
 *   RC522 GND  -> GND
 *
 * On other boards, swap the pin numbers for whatever's convenient —
 * any 5 GPIOs work for software SPI.
 */

#include <NiusWireless.h>

NiusRC522 rfid(SDA, 10, SCL, 11, 12);   // CS, RST, SCK, MOSI, MISO

void setup() {
    Serial.begin(9600);
    delay(1500);                       // USB-CDC enumerate
    rfid.begin();
    Serial.println(F("Hold a card on the reader, then remove it."));
}

void loop() {
    if (!rfid.cardPresent()) return;  // single-shot per physical tap
    rfid.printInfo();                  // UID / ATQA / SAK / type
    rfid.halt();                       // tag goes to HALT state
    delay(1000);                       // debounce so a stuck card doesn't spam
}
