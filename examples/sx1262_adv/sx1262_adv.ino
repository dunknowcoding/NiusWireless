/*
 * sx1262_adv — SX1262 LoRa advanced example
 *
 * Demonstrates full SX1262 feature set:
 *   - Custom frequency, BW, SF, CR, power
 *   - DC-DC regulator selection
 *   - TCXO configuration
 *   - DIO1 interrupt-driven RX
 *   - DIO2 as RF switch
 *   - CAD (Channel Activity Detection)
 *   - Sleep and wake
 *   - Raw register access
 *
 * --- Wiring ---
 *   SX1262 NSS  → D10   SX1262 RST  → D9
 *   SX1262 BUSY → D8    SX1262 DIO1 → D2
 *   SCK → D13   MOSI → D11   MISO → D12
 *   VCC → 3.3 V          GND → GND
 */

#include <NiusWireless.h>

#define SX1262_CS_PIN    10
#define SX1262_RST_PIN    9
#define SX1262_BUSY_PIN   8
#define SX1262_DIO1_PIN   2

NiusSX1262 radio(SX1262_CS_PIN, SX1262_RST_PIN, SX1262_BUSY_PIN, SX1262_DIO1_PIN);

volatile bool irqFired = false;

void onDIO1() {
    irqFired = true;
}

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println("NiusWireless - SX1262 Advanced Example");
    Serial.println("---------------------------------------");

    if (!radio.begin(915.0)) {
        Serial.println("ERROR: SX1262 not found.");
        while (1) { delay(500); }
    }

    // Use DC-DC converter for lower current draw (requires external inductor)
    radio.setRegulatorMode(true);

    // Route DIO2 to control the module's built-in RF switch (EBYTE E22, etc.)
    radio.setDIO2AsRFSwitch(true);

    // Custom LoRa parameters for longer range
    radio.setBandwidth(125000);    // 125 kHz
    radio.setSpreadingFactor(10);  // SF10 — good range, moderate throughput
    radio.setCodingRate(5);        // 4/5
    radio.setTxPower(22);          // Maximum 22 dBm (SX1262 / SX1268)
    radio.setPreambleLength(8);
    radio.setSyncWord(0x12);       // Private network
    radio.enableCRC();

    Serial.print("Version: ");
    Serial.println(radio.getVersion());
    Serial.println("Config: 915 MHz, BW125, SF10, CR4/5, 22 dBm");

    // Attach interrupt on DIO1
    attachInterrupt(digitalPinToInterrupt(SX1262_DIO1_PIN), onDIO1, RISING);

    radio.startReceive();
    Serial.println("Listening...");
    Serial.println();
}

void loop() {
    // ---- TX every 6 s -----------------------------------------------
    static unsigned long lastTx = 0;
    if (millis() - lastTx >= 6000) {
        // CAD before transmitting
        if (radio.isChannelActive()) {
            Serial.println("Channel busy — skipping TX.");
        } else {
            radio.beginPacket();
            radio.writeStr("Hello from SX1262 ADV");
            radio.endPacket(false);
            Serial.println("TX done.");
            radio.startReceive();
        }
        lastTx = millis();
    }

    // ---- IRQ-driven RX ----------------------------------------------
    if (irqFired) {
        irqFired = false;
        radio.handleDIO1();

        uint8_t pktLen = radio.parsePacket();
        if (pktLen > 0) {
            Serial.print("RX (");
            Serial.print(pktLen);
            Serial.print(" B): ");
            uint8_t buf[128];
            uint8_t n = radio.readBuf(buf, sizeof(buf) - 1);
            buf[n] = 0;
            Serial.println((char *)buf);
            Serial.print("  RSSI=");
            Serial.print(radio.getRSSI());
            Serial.print(" dBm  SNR=");
            Serial.print(radio.getSNR());
            Serial.println(" dB");
        }
    }

    // ---- Print ambient RSSI every 10 s ------------------------------
    static unsigned long lastRssi = 0;
    if (millis() - lastRssi >= 10000) {
        Serial.print("Ambient RSSI: ");
        Serial.print(radio.getLastRSSI());
        Serial.println(" dBm");
        lastRssi = millis();
    }
}
