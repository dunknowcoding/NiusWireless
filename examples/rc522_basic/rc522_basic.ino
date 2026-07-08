/*
 * rc522_basic — Simplest RC522 card reader example
 *
 * Scans for RFID cards and prints the UID and card type to Serial Monitor.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)   chip-select, software SPI
 *   RC522 SCK  -> SCL  (D19 / A5)   software SPI clock
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 IRQ  -> D13              (not used in this sketch)
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 *
 * --- Supported boards ---
 *   Any board supported by the NiusWireless library.
 *   Change the pin numbers to match your wiring.
 */

#include <NiusWireless.h>

// --- Pin definitions ---
// Using SDA/SCL macros so this sketch works across boards.
// On UNO R4 WiFi: SDA=D18(A4), SCL=D19(A5)
#define RC522_CS_PIN    SDA   // chip-select
#define RC522_RST_PIN   10    // reset
#define RC522_SCK_PIN   SCL   // software SPI clock
#define RC522_MOSI_PIN  11    // MOSI
#define RC522_MISO_PIN  12    // MISO

// Create the RC522 object using software SPI
NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

void setup() {
    Serial.begin(9600);

    // Wait for Serial Monitor to open (needed on native-USB boards)
    while (!Serial) {
        delay(10);
    }

    Serial.println("NiusWireless — RC522 Basic Example");
    Serial.println("-----------------------------------");

    if (!rfid.begin()) {
        Serial.println("ERROR: RC522 not found. Check wiring and power.");
        while (1) {
            delay(500);
        }
    }

    Serial.print("RC522 ready. Firmware: ");
    Serial.println(rfid.getVersion());
    Serial.println("Bring a card or keychain near the sensor...");
    Serial.println();
}

void loop() {
    // cardPresent() returns true when a card is detected and selected.
    // UID and card type are stored inside the rfid object.
    if (rfid.cardPresent()) {
        Serial.print("UID: ");
        Serial.println(rfid.getUID());

        Serial.print("Type: ");
        Serial.println(rfid.getCardTypeName());

        Serial.println();

        // Send HALT so we do not keep re-reading the same card on the next loop.
        rfid.halt();
        delay(500);
    }
}
