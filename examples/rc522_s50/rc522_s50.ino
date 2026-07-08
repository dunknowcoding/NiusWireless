/*
 * rc522_s50 — MIFARE Classic 1K (S50) read and write example
 *
 * Demonstrates reading and writing 16-byte blocks on a MIFARE Classic 1K
 * (S50) card using Key A authentication.
 *
 * MIFARE Classic 1K structure:
 *   16 sectors x 4 blocks = 64 blocks total
 *   Block  0       : manufacturer data (READ ONLY — do not write)
 *   Blocks 1-2     : data blocks in sector 0
 *   Block  3       : sector 0 trailer (Key A | Access Bits | Key B)
 *   Blocks 4-6     : data blocks in sector 1  <-- this sketch uses block 4
 *   Block  7       : sector 1 trailer
 *   ... and so on.
 *
 * Default factory Key A and Key B: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
 *
 * WARNING:
 *   Do NOT write to block 0 (manufacturer block).
 *   Do NOT write to sector trailer blocks (3, 7, 11, ... 63) unless you
 *   know exactly what you are doing — incorrect access bits can LOCK the
 *   sector permanently.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)
 *   RC522 SCK  -> SCL  (D19 / A5)
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 IRQ  -> D13  (not used in this sketch)
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 */

#include <NiusWireless.h>

// --- Pin definitions ---
#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12

NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

// Block to read/write — block 4 is the first data block of sector 1
#define TARGET_BLOCK  4

// Default factory key (all 0xFF)
uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Data to write (16 bytes exactly)
uint8_t writeData[16] = {
    'N', 'i', 'u', 's', 'W', 'i', 'r', 'e',
    'l', 'e', 's', 's', ' ', 'v', '1', '0'
};

// Buffer for read-back data
uint8_t readData[16];

void printBlock(uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (data[i] < 0x10) { Serial.print("0"); }
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — RC522 MIFARE Classic 1K (S50) Example");
    Serial.println("------------------------------------------------------");

    if (!rfid.begin()) {
        Serial.println("ERROR: RC522 not found. Check wiring.");
        while (1) { delay(500); }
    }

    Serial.print("RC522 ready: ");
    Serial.println(rfid.getVersion());
    Serial.println("Bring your S50 card near the sensor...");
    Serial.println();
}

void loop() {
    if (!rfid.cardPresent()) {
        return;
    }

    Serial.print("Card detected! UID: ");
    Serial.println(rfid.getUID());
    Serial.print("Type: ");
    Serial.println(rfid.getCardTypeName());

    // Only work with MIFARE Classic 1K cards
    if (rfid.getCardType() != NIUS_CARD_MIFARE_1K) {
        Serial.println("Not a MIFARE Classic 1K card. Skipping.");
        rfid.halt();
        delay(1000);
        return;
    }

    // --- Authenticate sector 1 using Key A ---
    Serial.print("Authenticating block ");
    Serial.print(TARGET_BLOCK);
    Serial.print(" with Key A... ");

    uint8_t status = rfid.authenticate(TARGET_BLOCK, NIUS_KEY_A, key);

    if (status != NIUS_OK) {
        Serial.println("FAILED. Wrong key or card error.");
        rfid.stopCrypto();
        rfid.halt();
        delay(1000);
        return;
    }
    Serial.println("OK");

    // --- Write 16 bytes to the target block ---
    Serial.print("Writing to block ");
    Serial.print(TARGET_BLOCK);
    Serial.print(": ");
    printBlock(writeData, 16);

    status = rfid.writeBlock(TARGET_BLOCK, writeData);
    if (status != NIUS_OK) {
        Serial.println("Write FAILED.");
    } else {
        Serial.println("Write OK");
    }

    // Some MIFARE Classic cards need a few ms after a write before they
    // can service the next command (the card's internal EEPROM write is
    // still completing). 5 ms is the typical "safe" delay.
    delay(5);

    // --- Read the block back and verify ---
    Serial.print("Reading block ");
    Serial.print(TARGET_BLOCK);
    Serial.print(": ");

    status = rfid.readBlock(TARGET_BLOCK, readData);
    if (status != NIUS_OK) {
        Serial.println("Read FAILED.");
    } else {
        printBlock(readData, 16);

        // Compare
        bool match = true;
        for (uint8_t i = 0; i < 16; i++) {
            if (readData[i] != writeData[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            Serial.println("Verify: PASS — data matches.");
        } else {
            Serial.println("Verify: FAIL — data mismatch!");
        }
    }

    // --- Clean up ---
    rfid.stopCrypto();
    rfid.halt();

    Serial.println("Done. Remove the card.");
    Serial.println();

    // Wait until card is removed before scanning again
    delay(2000);
}
