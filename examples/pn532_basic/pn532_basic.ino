/*
 * pn532_basic — PN532 NFC/RFID reader basic example
 *
 * Detects NFC cards/tags and prints the UID to the Serial Monitor.
 * Works with MIFARE Classic, MIFARE Ultralight, and other ISO 14443A tags.
 *
 * --- Wiring (I2C mode — default) ---
 *   PN532 VCC -> 3.3V  (or 5V if your module has onboard regulator)
 *   PN532 GND -> GND
 *   PN532 SDA -> SDA
 *   PN532 SCL -> SCL
 *   PN532 IRQ -> D2   (interrupt — recommended for faster response)
 *   PN532 RST -> D3
 *
 *   On the PN532 breakout, set DIP switches / jumpers to I2C mode:
 *   SEL0=LOW, SEL1=HIGH (check your module's datasheet).
 *
 * NOTE: The PN532 driver is a stub in this release.
 *       This sketch compiles but does not scan until the full driver
 *       is available. Check the NiusWireless releases.
 */

#include <NiusWireless.h>

#define PN532_IRQ_PIN  2
#define PN532_RST_PIN  3

// I2C constructor: IRQ pin, RST pin
NiusPN532 nfc(PN532_IRQ_PIN, PN532_RST_PIN);

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — PN532 Basic Example");
    Serial.println("-----------------------------------");

    if (!nfc.begin()) {
        Serial.println("ERROR: PN532 not found. Check wiring and I2C mode setting.");
        while (1) { delay(500); }
    }

    Serial.print("PN532 ready: ");
    Serial.println(nfc.getVersion());
    Serial.println("Bring a card or tag near the sensor...");
    Serial.println();
}

void loop() {
    if (nfc.cardPresent()) {
        Serial.print("Tag UID: ");
        Serial.println(nfc.getUID());
        Serial.println();
        delay(500);
    }
}
