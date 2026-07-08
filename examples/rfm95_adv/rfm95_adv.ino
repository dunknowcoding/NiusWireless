/*
 * rfm95_adv — RFM95W LoRa advanced example
 *
 * Demonstrates full control of the RFM95W module:
 *   - Custom frequency, bandwidth, spreading factor, coding rate
 *   - TX power selection
 *   - Sync word (private vs LoRaWAN)
 *   - Preamble length
 *   - CRC enable/disable
 *   - Interrupt-driven RX using DIO0
 *   - Channel Activity Detection (CAD)
 *   - RSSI, SNR and frequency error
 *   - LNA gain override
 *   - Sleep and wake
 *
 * --- Wiring (any UNO-form-factor board) ---
 *   RFM95W label → Arduino pin
 *   GND  → GND
 *   VCC  → 3.3 V
 *   SCK  → D13
 *   MISO → D12
 *   MOSI → D11
 *   CS   → D10
 *   RST  → D9
 *   DIO0 → D2     (attach interrupt below)
 */

#include <NiusWireless.h>

#define RFM95_CS_PIN    10
#define RFM95_RST_PIN    9
#define RFM95_DIO0_PIN   2

NiusRFM95 radio(RFM95_CS_PIN, RFM95_RST_PIN, RFM95_DIO0_PIN);

// Flag set inside the interrupt service routine
volatile bool packetReady = false;

// Interrupt service routine — called when DIO0 goes HIGH
void onDIO0() {
    radio.handleDIO0();
    packetReady = true;
}

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println("NiusWireless - RFM95W Advanced Example");
    Serial.println("---------------------------------------");

    // Initialise at 915 MHz
    if (!radio.begin(915.0)) {
        Serial.println("ERROR: RFM95W not found.");
        while (1) { delay(500); }
    }

    // --- Custom modulation settings ---
    radio.setBandwidth(125000);    // 125 kHz
    radio.setSpreadingFactor(9);   // SF9 (longer range, slower)
    radio.setCodingRate(5);        // 4/5
    radio.setTxPower(14);          // 14 dBm (use 20 for maximum range)
    radio.setPreambleLength(8);    // 8 symbols (default)
    radio.setSyncWord(0x12);       // 0x12 = private, 0x34 = LoRaWAN
    radio.enableCRC();
    radio.setExplicitHeader();
    radio.setLNAGain(0);           // 0 = auto AGC (recommended)

    Serial.print("Version: ");
    Serial.println(radio.getVersion());

    // Attach DIO0 interrupt for non-blocking RX
    attachInterrupt(digitalPinToInterrupt(RFM95_DIO0_PIN), onDIO0, RISING);

    // Start continuous receive
    radio.startReceive();

    Serial.println("Listening (SF9, BW125, 915 MHz)...");
    Serial.println();
}

void loop() {
    // ---- Send a test packet every 5 s --------------------------------
    static unsigned long lastTx = 0;
    if (millis() - lastTx >= 5000) {
        // Check channel before transmitting (CAD)
        if (radio.isChannelActive()) {
            Serial.println("Channel busy, skipping TX.");
        } else {
            radio.beginPacket();
            radio.writeStr("NiusLoRa ADV");
            radio.endPacket(false);  // false = blocking

            Serial.println("Packet sent.");

            // Restart RX after TX
            radio.startReceive();
        }
        lastTx = millis();
    }

    // ---- Non-blocking RX via interrupt flag --------------------------
    if (packetReady) {
        packetReady = false;

        uint8_t pktLen = radio.parsePacket();
        if (pktLen > 0) {
            Serial.print("Received (");
            Serial.print(pktLen);
            Serial.print(" B): ");

            uint8_t buf[64];
            uint8_t n = radio.readBuf(buf, sizeof(buf) - 1);
            buf[n] = 0;
            Serial.println((char *)buf);

            Serial.print("  RSSI: ");
            Serial.print(radio.getRSSI());
            Serial.print(" dBm   SNR: ");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
        }
    }

    // ---- Print ambient RSSI every 10 s --------------------------------
    static unsigned long lastRssiPrint = 0;
    if (millis() - lastRssiPrint >= 10000) {
        Serial.print("Ambient RSSI: ");
        Serial.print(radio.getLastRSSI());
        Serial.println(" dBm");
        lastRssiPrint = millis();
    }
}
