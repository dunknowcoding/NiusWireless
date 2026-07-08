/*
 * rc522_basic — RC522 type explorer
 *
 * Scans for RFID cards and prints the UID, ATQA, SAK and detected card
 * type to Serial Monitor. For Ultralight / NTAG tags it also sends
 * GET_VERSION (0x60) to read the tag's product / vendor bytes and
 * identifies the specific chip (NTAG213/215/216, Ultralight EV1, etc.).
 *
 * Also recommends which example to run for the detected card family.
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

static void printLine() {
    Serial.println(F("---------------------------------------------------------------"));
}

void setup() {
    Serial.begin(9600);

    // Give USB CDC time to enumerate (avoids blocking on while(!Serial))
    delay(1500);

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
    // cardPresentWake() uses WUPA — detects both new cards (IDLE) and
    // previously halted cards (HALT state), giving reliable detection when
    // tags are permanently in the field.
    if (rfid.cardPresentWake()) {
        Serial.println();
        printLine();
        Serial.println(F("Tag detected"));
        printLine();
        Serial.print(F("  UID:   ")); Serial.println(rfid.getUID());
        Serial.print(F("  ATQA:  ")); Serial.println(rfid.getATQA());
        Serial.print(F("  SAK:   0x"));
        if (rfid.getSAK() < 0x10) Serial.print('0');
        Serial.println(rfid.getSAK(), HEX);
        Serial.print(F("  Type:  ")); Serial.println(rfid.getCardTypeName());

        // For Ultralight / NTAG, ask the tag for its GET_VERSION (0x60)
        // response to distinguish the actual product (NTAG213/215/216,
        // Ultralight, Ultralight EV1, etc.) — ATQA+SAK alone can't.
        if (rfid.getCardType() == NIUS_CARD_MIFARE_UL) {
            uint8_t v[8];
            if (rfid.getNTAGVersion(v) == NIUS_OK) {
                Serial.print(F("  Tag:   "));
                // vendor: 0x04 = NXP; product: 0x03 = Ultralight, 0x04 = NTAG
                if (v[1] == 0x04) {
                    if (v[2] == 0x04) {
                        Serial.print(F("NXP NTAG"));
                        if (v[3] == 0x02) Serial.print(F("213"));
                        else if (v[3] == 0x11) Serial.print(F("215"));
                        else if (v[3] == 0x13) Serial.print(F("216"));
                        else                              Serial.print(F(" (subtype 0x"));
                        if (v[3] >= 0x10) {
                            Serial.print(v[3], HEX); Serial.print(')');
                        } else {
                            Serial.print(F("2"));
                            Serial.print(v[3], HEX);
                        }
                    } else if (v[2] == 0x03) {
                        Serial.print(F("NXP MIFARE Ultralight"));
                        if (v[3] == 0x01) Serial.print(F(" EV1"));
                        if (v[3] == 0x02) Serial.print(F(" C"));
                    } else {
                        Serial.print(F("NXP product 0x"));
                        Serial.print(v[2], HEX);
                    }
                    Serial.print(F(" v"));
                    Serial.print(v[4], DEC);
                    Serial.print('.');
                    Serial.print(v[5], DEC);
                } else {
                    Serial.print(F("vendor 0x"));
                    Serial.print(v[1], HEX);
                }
                Serial.println();
            } else {
                Serial.println(F("  Tag:   GET_VERSION did not respond (tag may be broken)."));
            }
        }

        // Recommend which example actually operates this kind of tag.
        Serial.println();
        switch (rfid.getCardType()) {
            case NIUS_CARD_MIFARE_1K:
            case NIUS_CARD_MIFARE_4K:
            case NIUS_CARD_MIFARE_MINI:
                Serial.println(F("  -> Run rc522_s50 or rc522_tag for full block read/write."));
                Serial.println(F("  -> If this is a Chinese magic card (CUID / Gen2 / Gen1a / Gen3),"));
                Serial.println(F("     rc522_tag step 0 will rewrite block 0 / change the UID."));
                break;
            case NIUS_CARD_MIFARE_UL:
                Serial.println(F("  -> MIFARE Ultralight / NTAG detected."));
                Serial.println(F("     rc522_tag (it has Ultralight page read/write), or an Android"));
                Serial.println(F("     app like Mifare Classic Tool / TagInfo / NFC Tools."));
                break;
            case NIUS_CARD_ISO14443_4:
            case NIUS_CARD_DESFIRE:
                Serial.println(F("  -> ISO 14443-4 / DESFire card."));
                Serial.println(F("     The NiusWireless library does not implement APDU-level"));
                Serial.println(F("     operations for these. Use PN532 or a phone app."));
                break;
            case NIUS_CARD_ISO18092:
                Serial.println(F("  -> ISO 18092 (NFC-IP1) card — peer-to-peer, not a tag."));
                break;
            case NIUS_CARD_MIFARE_PLUS:
                Serial.println(F("  -> MIFARE Plus card."));
                Serial.println(F("     Classic emulation layer (SL1) may work; SL2/SL3 needs"));
                Serial.println(F("     AES — not implemented in NiusWireless."));
                break;
            case NIUS_CARD_TNP3XXX:
            case NIUS_CARD_UNKNOWN:
            default:
                Serial.println(F("  -> Unknown type. No NiusWireless example matches."));
                break;
        }

        // Send HALT so we do not keep re-reading the same card on the next loop.
        rfid.halt();
        delay(500);
    }
}
