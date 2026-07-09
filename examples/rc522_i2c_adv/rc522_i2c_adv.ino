/*
 * rc522_i2c_adv - RC522 advanced example over I2C.
 *
 * Auto-detects the card family and runs the appropriate operations:
 *
 *   MIFARE Classic 1K / 4K / Mini :
 *     - dumpToSerial() reads every block the factory key can read
 *     - authenticate -> readBlock(4) -> writeBlock(0) = "NiusI2C!" (rounds the
 *       UID with a string marker, on CUID cards; stock Classic keeps the
 *       original content). Original content restored before exit.
 *
 *   MIFARE Ultralight / NTAG :
 *     - dumpToSerial() reads every page that responds
 *     - readPage(0) and writePage(0) roundtrip with a marker byte
 *
 *   Other families : a one-liner telling you which tool / library to use.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   MFRC522 chip SDA  -> board SDA  (D18 / A4)
 *   MFRC522 chip SCL  -> board SCL  (D19 / A5)
 *   RC522 RST         -> D10
 *   RC522 3.3V        -> 3.3V
 *   RC522 GND         -> GND
 *
 *   MFRC522 I2C default address: 0x28 (some boards: 0x29).
 */

#include <NiusWireless.h>

#define RC522_I2C_ADDRESS 0x28
#define RC522_RST_PIN     10

NiusRC522 rfid(Wire, RC522_I2C_ADDRESS, RC522_RST_PIN);

static uint8_t savedBlock[16];
static uint8_t savedPage[4];

void op_Classic() {
    Serial.println();
    Serial.println(F("--- MIFARE Classic 1K / 4K / Mini ---"));
    rfid.dumpToSerial();

    if (rfid.authenticate(4, NIUS_KEY_A, (uint8_t *)NIUS_KEY_DEFAULT) != NIUS_OK) {
        Serial.println(F("(auth on sector 1 with factory key failed)"));
        rfid.stopCrypto();
        return;
    }

    if (rfid.readBlock(4, savedBlock) != NIUS_OK) {
        rfid.stopCrypto();
        return;
    }
    uint8_t marker[16] = {
        'N', 'i', 'u', 's', 'I', '2', 'C', '!',
        ' ', ':', ')', ' ', 0x00, 0x00, 0x00, 0x00
    };
    if (rfid.writeBlock(4, marker) == NIUS_OK) {
        uint8_t tmp[16];
        if (rfid.readBlock(4, tmp) == NIUS_OK) {
            Serial.print(F("read-back: "));
            for (uint8_t i = 0; i < 16; i++) {
                if (i) Serial.print(' ');
                if (tmp[i] < 0x10) Serial.print('0');
                Serial.print(tmp[i], HEX);
            }
            Serial.println();
        }
        rfid.writeBlock(4, savedBlock);
        Serial.println(F("(original block 4 restored)"));
    }
    rfid.stopCrypto();
}

void op_Ultralight() {
    Serial.println();
    Serial.println(F("--- MIFARE Ultralight / NTAG ---"));
    rfid.dumpToSerial();

    if (rfid.readPage(0, savedPage) != NIUS_OK) { return; }
    uint8_t marker[4] = { 'N', 'I', '!', 0x00 };
    if (rfid.writePage(0, marker) == NIUS_OK) {
        uint8_t tmp[16];
        if (rfid.readPage(0, tmp) == NIUS_OK) {
            Serial.print(F("read-back: "));
            for (uint8_t i = 0; i < 4; i++) {
                if (i) Serial.print(' ');
                if (tmp[i] < 0x10) Serial.print('0');
                Serial.print(tmp[i], HEX);
            }
            Serial.println();
        }
        rfid.writePage(0, savedPage);
        Serial.println(F("(original page 0 restored)"));
    }
}

void op_Unsupported() {
    Serial.println(F("(card family not supported by NiusRC522 - try PN532 or an Android app)"));
}

void setup() {
    Serial.begin(9600);
    delay(1500);
    rfid.begin();
    Serial.println(F("Hold an S50 / Ultralight tag near the reader..."));
}

void loop() {
    if (!rfid.cardPresentWake()) return;  // also wakes HALT'd cards
    rfid.printInfo();

    switch (rfid.getCardType()) {
        case NIUS_CARD_MIFARE_1K:
        case NIUS_CARD_MIFARE_4K:
        case NIUS_CARD_MIFARE_MINI:  op_Classic();    break;
        case NIUS_CARD_MIFARE_UL:    op_Ultralight(); break;
        default:                     op_Unsupported(); break;
    }
    rfid.halt();
}
