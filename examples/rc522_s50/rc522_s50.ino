/*
 * rc522_s50 — MIFARE Classic 1K (S50) read/write demo
 *
 * Authenticates block 4 with the factory key, writes a recognisable
 * payload, reads it back, and restores the original content.
 *
 * Use this to verify your reader + wiring work end-to-end on a
 * healthy S50 (or any compatible 1K card).
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   Same as rc522_basic.
 */

#include <NiusWireless.h>

NiusRC522 rfid(SDA, 10, SCL, 11, 12);

static uint8_t origBlock[16];          // remember the original content

void printBlock(uint8_t *data) {
    for (uint8_t i = 0; i < 16; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void setup() {
    Serial.begin(9600);
    delay(1500);
    rfid.begin();
    rfid.setAntennaGain(NIUS_GAIN_48DB);
    Serial.println(F("Hold an S50 card on the reader..."));
}

void loop() {
    if (!rfid.cardPresentWake()) return;

    if (rfid.getCardType() != NIUS_CARD_MIFARE_1K) {
        Serial.println(F("Not a 1K card — skipping."));
        rfid.halt();
        return;
    }

    if (rfid.authenticate(4, NIUS_KEY_A, (uint8_t *)NIUS_KEY_DEFAULT) != NIUS_OK) {
        Serial.println(F("Auth failed. Wrong key?"));
        rfid.stopCrypto(); rfid.halt();
        return;
    }

    // Read original (so we can restore).
    rfid.readBlock(4, origBlock);

    uint8_t writeData[16] = {
        'N', 'i', 'u', 's', 'W', 'i', 'r', 'e',
        'l', 'e', 's', 's', ' ', 'v', '1', '0'
    };

    rfid.writeBlock(4, writeData);
    delay(5);                         // give EEPROM time to commit

    uint8_t readData[16];
    rfid.readBlock(4, readData);
    Serial.print(F("Read-back: "));
    printBlock(readData);

    // Restore original.
    rfid.writeBlock(4, origBlock);
    Serial.println(F("OK."));

    rfid.stopCrypto();
    rfid.halt();
}
