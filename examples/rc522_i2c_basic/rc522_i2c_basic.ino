/*
 * rc522_i2c_basic - Minimal RC522 example over I2C.
 *
 * Detects a tag on the reader and prints its UID / type. The smallest
 * sketch that exercises the I2C transport - about 30 lines.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   MFRC522 chip SDA  -> board SDA  (D18 / A4)
 *   MFRC522 chip SCL  -> board SCL  (D19 / A5)
 *   RC522 RST         -> D10
 *   RC522 3.3V        -> 3.3V
 *   RC522 GND         -> GND
 *
 * The chip must be wired for I2C mode (its I2C/SPI pin must select
 * the I2C bus). MFRC522 default address is 0x28 (I2C_ADD0 LOW); use
 * 0x29 if your board ties I2C_ADD0 to VCC.
 *
 * --- Try it ---
 *   1. Place a card on the reader.
 *   2. Open Serial Monitor at 9600 baud.
 *   3. Watch UID / ATQA / SAK / Type get printed.
 */

#include <NiusWireless.h>

#define RC522_I2C_ADDRESS 0x28   // 0x28 by default; some boards use 0x29
#define RC522_RST_PIN     10

NiusRC522 rfid(Wire, RC522_I2C_ADDRESS, RC522_RST_PIN);  // I2C constructor

void setup() {
    Serial.begin(9600);
    delay(1500);                              // USB-CDC enumerate
    Wire.begin();                             // already initialised by rfid.begin()
    rfid.begin();
    Serial.println(F("Hold an S50 / Ultralight tag near the reader..."));
}

void loop() {
    if (!rfid.cardPresent()) return;
    rfid.printInfo();                          // UID / ATQA / SAK / Type
    rfid.halt();
}
