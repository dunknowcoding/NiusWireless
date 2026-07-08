/*
 * sx1262_basic — SX1262 LoRa basic example
 *
 * Demonstrates sending and receiving LoRa packets with an SX1262-based
 * module (e.g. EBYTE E22-900M22S, Ai-Thinker Ra-06).
 *
 * Key difference from SX127x: the SX126x needs a BUSY pin that must be
 * LOW before every SPI command.
 *
 * --- Wiring (any UNO-form-factor board) ---
 *   SX1262 label   → Arduino pin
 *   GND            → GND
 *   VCC            → 3.3 V
 *   SCK            → D13
 *   MOSI           → D11
 *   MISO           → D12
 *   NSS / CS       → D10
 *   NRESET / RST   → D9
 *   BUSY           → D8
 *   DIO1           → D2    (interrupt for TxDone / RxDone)
 *   ANT            → antenna (always required before TX!)
 *
 * --- Supported boards ---
 *   Any board supported by the NiusWireless library.
 */

#include <NiusWireless.h>

// Set to 0 for transmitter, 1 for receiver
#define ROLE  0

// Frequency in MHz
#define LORA_FREQ  915.0   // use 433.0 for SX1268-based modules

// Pin definitions
#define SX1262_CS_PIN    10
#define SX1262_RST_PIN    9
#define SX1262_BUSY_PIN   8
#define SX1262_DIO1_PIN   2

// Create the radio object (CS, RST, BUSY, DIO1)
NiusSX1262 radio(SX1262_CS_PIN, SX1262_RST_PIN, SX1262_BUSY_PIN, SX1262_DIO1_PIN);

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println("NiusWireless - SX1262 Basic Example");
    Serial.println("------------------------------------");

    if (!radio.begin(LORA_FREQ)) {
        Serial.println("ERROR: SX1262 not found. Check wiring and BUSY/DIO1 pins.");
        while (1) {
            delay(500);
        }
    }

    // Enable DIO2 as RF switch control (required on many SX1262 modules)
    radio.setDIO2AsRFSwitch(true);

    Serial.print("SX1262 ready: ");
    Serial.println(radio.getVersion());
    Serial.print("Frequency: ");
    Serial.print(LORA_FREQ);
    Serial.println(" MHz");

    if (ROLE == 0) {
        Serial.println("Role: TRANSMITTER");
    } else {
        Serial.println("Role: RECEIVER");
        radio.startReceive();
    }
    Serial.println();
}

void loop() {
    if (ROLE == 0) {
        // ---- Transmitter -----------------------------------------------
        static uint32_t counter = 0;

        radio.beginPacket();
        radio.writeStr("SX1262 packet #");

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

        // Restart RX after TX so we can hear replies
        radio.startReceive();
        delay(2000);

    } else {
        // ---- Receiver --------------------------------------------------
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
