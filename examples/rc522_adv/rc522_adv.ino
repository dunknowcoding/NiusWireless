/*
 * rc522_adv — RC522 advanced example
 *
 * Demonstrates full control of the RC522 module:
 *   - Adjustable SPI speed (hardware SPI mode)
 *   - Custom antenna gain
 *   - Interrupt-driven card detection (IRQ pin)
 *   - Raw register read/write
 *   - Antenna on/off control
 *   - Full UID byte array access
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)   chip-select, software SPI
 *   RC522 SCK  -> SCL  (D19 / A5)   software SPI clock
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 IRQ  -> D13
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 *
 * --- Supported boards ---
 *   Any board supported by the NiusWireless library.
 */

#include <NiusWireless.h>

// --- Pin definitions ---
#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12
#define RC522_IRQ_PIN   13

// Software SPI constructor (CS, RST, SCK, MOSI, MISO)
NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

// Flag set inside the interrupt service routine
volatile bool cardDetected = false;

// Interrupt service routine — called when IRQ goes LOW
void onCardIRQ() {
    cardDetected = true;
}

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — RC522 Advanced Example");
    Serial.println("--------------------------------------");

    // begin() also accepts a custom SPI speed for hardware SPI mode.
    // In software SPI mode this parameter is ignored but is still accepted.
    if (!rfid.begin(4000000UL)) {
        Serial.println("ERROR: RC522 not found.");
        while (1) { delay(500); }
    }

    // Set maximum receiver gain for best range
    rfid.setAntennaGain(NIUS_GAIN_48DB);
    Serial.print("Antenna gain set to: 0x");
    Serial.println(rfid.getAntennaGain(), HEX);

    // Attach the interrupt pin
    rfid.setIRQPin(RC522_IRQ_PIN);
    attachInterrupt(digitalPinToInterrupt(RC522_IRQ_PIN), onCardIRQ, FALLING);

    // Read the raw version register for demonstration
    uint8_t ver = rfid.readRegister(0x37);
    Serial.print("Version register (0x37): 0x");
    Serial.println(ver, HEX);

    Serial.print("Firmware string: ");
    Serial.println(rfid.getVersion());
    Serial.println();
    Serial.println("Waiting for card (IRQ driven)...");
}

void loop() {
    // Check interrupt flag instead of polling
    if (cardDetected) {
        cardDetected = false;

        if (rfid.cardPresent()) {
            // Get raw UID bytes
            uint8_t uidBuf[NIUS_UID_MAX_LEN];
            uint8_t uidLen = 0;
            rfid.getUIDBytes(uidBuf, uidLen);

            Serial.print("UID bytes (");
            Serial.print(uidLen);
            Serial.print("): ");
            for (uint8_t i = 0; i < uidLen; i++) {
                if (uidBuf[i] < 0x10) { Serial.print("0"); }
                Serial.print(uidBuf[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            Serial.print("UID string: ");
            Serial.println(rfid.getUID());

            Serial.print("Card type code: 0x");
            Serial.println(rfid.getCardType(), HEX);

            Serial.print("Card type name: ");
            Serial.println(rfid.getCardTypeName());

            // Demonstrate antenna off / on
            rfid.antennaOff();
            delay(100);
            rfid.antennaOn();

            rfid.halt();
            Serial.println();
        }
    }
}
