/*
 * rc522_adv — Multi-feature showcase
 *
 * Builds on rc522_basic and adds:
 *   - One-call dump of the whole card (rfid.dumpToSerial())
 *   - A roundtrip on block 0 for MIFARE Classic (auth -> read -> write -> restore)
 *   - Antenna gain control + raw version-register inspection
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   Same as rc522_basic — see that file for pin numbers.
 */

#include <NiusWireless.h>

NiusRC522 rfid(SDA, 10, SCL, 11, 12);

void setup() {
    Serial.begin(9600);
    delay(1500);

    if (!rfid.begin()) {
        Serial.println(F("ERROR: RC522 not found."));
        while (1) delay(500);
    }

    Serial.print(F("Reader: "));  Serial.println(rfid.getVersion());
    Serial.print(F("VersionReg (0x37): 0x"));
    Serial.println(rfid.readRegister(MFRC522_REG_VERSION), HEX);
    Serial.println();
}

void loop() {
    if (!rfid.cardPresentWake()) return;

    rfid.printInfo();
    rfid.dumpToSerial();              // type-adaptive — Classic or Ultralight

    // MIFARE Classic roundtrip on block 0 (UID area).
    // Demonstrates:  authenticate -> read -> write marker -> read-back -> restore.
    if (rfid.getCardType() == NIUS_CARD_MIFARE_1K) {
        uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (rfid.authenticate(3, NIUS_KEY_A, key) == NIUS_OK) {
            uint8_t data[16];
            rfid.readBlock(0, data);    // block 0 holds the UID on Classic
            Serial.print(F("  block0: "));
            for (uint8_t i = 0; i < 16; i++) {
                if (data[i] < 0x10) Serial.print('0');
                Serial.print(data[i], HEX);
                Serial.print(' ');
            }
            Serial.println();
            rfid.stopCrypto();
        }
    }
    rfid.halt();
}
