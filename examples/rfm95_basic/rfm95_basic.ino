/*
 * rfm95_basic — RFM95W LoRa basic transceiver example
 *
 * Demonstrates sending and receiving LoRa packets with the RFM95W module
 * (SX1276, 868/915 MHz).  Two boards each running this sketch will
 * communicate with each other.
 *
 * Set ROLE = 0 on one board (transmitter) and ROLE = 1 on the other (receiver),
 * or leave both as ROLE = 0 to have them take turns.
 *
 * --- Wiring (any UNO-form-factor board) ---
 *   RFM95W / Adafruit breakout label → Arduino pin
 *   GND      → GND
 *   Vin/VCC  → 3.3 V  (DO NOT connect to 5 V!)
 *   SCK      → D13    (hardware SPI SCK)
 *   MISO     → D12
 *   MOSI     → D11
 *   CS / NSS → D10
 *   RST      → D9
 *   G0/DIO0  → D2     (interrupt for TxDone / RxDone)
 *   ANT      → wire antenna (86 mm for 915 MHz, 164 mm for 433 MHz)
 *
 * --- Supported boards ---
 *   Any board supported by the NiusWireless library.
 *   Adjust pin numbers below for your board.
 *
 * NOTE: The RFM95W / SX1276 chip is 3.3 V only.  On a 5 V Arduino,
 *       add a level shifter on SCK, MOSI, and CS, or use a 3.3 V board.
 */

#include <NiusWireless.h>

// Set to 0 for transmitter, 1 for receiver
#define ROLE  0

// Frequency — change to match your module and local regulations
#define LORA_FREQ  915.0   // MHz  (use 433.0 for RFM96W / RA-01)

// Pin definitions
#define RFM95_CS_PIN    10
#define RFM95_RST_PIN    9
#define RFM95_DIO0_PIN   2

// Create the radio object
NiusRFM95 radio(RFM95_CS_PIN, RFM95_RST_PIN, RFM95_DIO0_PIN);

void setup() {
    Serial.begin(9600);

    // Wait for Serial Monitor on native-USB boards
    delay(1500);

    Serial.println("NiusWireless - RFM95W Basic Example");
    Serial.println("------------------------------------");

    if (!radio.begin(LORA_FREQ)) {
        Serial.println("ERROR: RFM95W not found. Check wiring and 3.3 V supply.");
        while (1) {
            delay(500);
        }
    }

    Serial.print("RFM95W ready: ");
    Serial.println(radio.getVersion());
    Serial.print("Frequency: ");
    Serial.print(LORA_FREQ);
    Serial.println(" MHz");

    if (ROLE == 0) {
        Serial.println("Role: TRANSMITTER — sending a packet every 2 s.");
    } else {
        Serial.println("Role: RECEIVER — listening for packets.");
        radio.startReceive();
    }
    Serial.println();
}

void loop() {
    if (ROLE == 0) {
        // ---- Transmitter -------------------------------------------
        static uint32_t counter = 0;

        radio.beginPacket();
        radio.writeStr("Hello from NiusLoRa #");
        // Write counter as ASCII digits (no advanced operators used)
        char buf[12];
        uint32_t tmp = counter;
        uint8_t len = 0;
        if (tmp == 0) {
            buf[len++] = '0';
        } else {
            char digits[12];
            uint8_t dlen = 0;
            while (tmp > 0) {
                digits[dlen++] = '0' + (char)(tmp % 10);
                tmp /= 10;
            }
            // Reverse
            for (uint8_t i = 0; i < dlen; i++) {
                buf[i] = digits[dlen - 1 - i];
            }
            len = dlen;
        }
        radio.writeBuf((uint8_t *)buf, len);
        radio.endPacket();

        Serial.print("Sent packet #");
        Serial.println(counter);
        counter++;

        delay(2000);

    } else {
        // ---- Receiver ----------------------------------------------
        uint8_t pktLen = radio.parsePacket();
        if (pktLen > 0) {
            Serial.print("Received ");
            Serial.print(pktLen);
            Serial.print(" bytes: ");

            while (radio.available()) {
                int b = radio.readByte();
                if (b >= 0) {
                    Serial.write((char)b);
                }
            }
            Serial.println();
            Serial.print("  RSSI: ");
            Serial.print(radio.getRSSI());
            Serial.print(" dBm   SNR: ");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
            Serial.println();
        }
    }
}
