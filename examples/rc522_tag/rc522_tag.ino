/*
 * rc522_tag — Type-adaptive tag operations
 *
 * Detects a card, dispatches to the right flow for its family, and
 * uses the high-level helpers so the loop body stays small:
 *
 *   MIFARE Classic 1K / 4K / Mini / CUID — authenticate + dump every
 *       block with the factory key, optionally change the UID on
 *       Chinese "magic" cards (setUid).
 *
 *   MIFARE Ultralight / NTAG — dump every page that responds,
 *       print the chip version if it's an EV1 / NTAG.
 *
 *   ISO 14443-4 / DESFire / Plus / ISO 18092 / unknown — print
 *       a one-liner recommending a different tool.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   Same as rc522_basic.
 */

#include <NiusWireless.h>

NiusRC522 rfid(SDA, 10, SCL, 11, 12);

void flow_Classic() {
    Serial.println();
    rfid.dumpToSerial();

    // Optional: try changing the UID +1 — accepted only on Chinese
    // "magic" cards (CUID / DirectWrite / FUID). Stock MIFARE Classic
    // cards keep their original UID.
    if (rfid.uidLen == 4) {
        uint8_t newUid[4] = { rfid.uid[0], rfid.uid[1], rfid.uid[2],
                              (uint8_t)(rfid.uid[3] + 1) };
        Serial.println();
        Serial.print(F("setUid(UID +1) -> "));
        Serial.println(rfid.setUid(newUid, 4) == NIUS_OK ? F("OK")
                                                        : F("rejected"));
    }
}

void flow_Ultralight() {
    Serial.println();
    rfid.dumpToSerial();

    uint8_t v[8];
    if (rfid.getNTAGVersion(v) == NIUS_OK) {
        Serial.print(F("GET_VERSION: vendor=0x"));
        Serial.print(v[1], HEX);
        Serial.print(F(" product=0x"));
        Serial.print(v[2], HEX);
        Serial.print(F(" subtype=0x"));
        Serial.println(v[3], HEX);
    }
}

void flow_Unsupported() {
    Serial.println(F("Card type not supported by NiusWireless — try PN532 or a phone app."));
}

void setup() {
    Serial.begin(9600);
    delay(1500);
    rfid.begin();
    Serial.println(F("Hold a card near the reader..."));
}

void loop() {
    if (!rfid.cardPresentWake()) return;
    rfid.printInfo();

    switch (rfid.getCardType()) {
        case NIUS_CARD_MIFARE_1K:
        case NIUS_CARD_MIFARE_4K:
        case NIUS_CARD_MIFARE_MINI: flow_Classic();   break;
        case NIUS_CARD_MIFARE_UL:   flow_Ultralight(); break;
        default:                    flow_Unsupported(); break;
    }
    rfid.halt();
}
