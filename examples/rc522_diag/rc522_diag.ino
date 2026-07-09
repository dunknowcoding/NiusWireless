/*
 * rc522_diag — Firmware health-check for NiusRC522.
 *
 * Use this sketch to determine whether the library + reader are working,
 * independent of any specific card. Place any tag on the reader and the
 * sketch will:
 *
 *   1. Initialise the chip, dump key configuration registers.
 *   2. Run REQA / anti-collision / SELECT, report UID / ATQA / SAK.
 *   3. Run a sensible roundtrip for the card type that came back:
 *        - MIFARE Classic (SAK top-bits 0x08): MIFARE_Auth + readBlock + writeBlock
 *          using the factory key (FFFFFFFFFFFF). Writes back original data.
 *        - MIFARE Ultralight (SAK 0x00): readPage(0), readPage(4),
 *          getNTAGVersion().
 *   4. Report each step's return code so you can see exactly where a
 *      card (or the firmware) misbehaves.
 *
 * --- Wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  -> SDA  (D18 / A4)
 *   RC522 SCK  -> SCL  (D19 / A5)
 *   RC522 MOSI -> D11
 *   RC522 MISO -> D12
 *   RC522 RST  -> D10
 *   RC522 3.3V -> 3.3V
 *   RC522 GND  -> GND
 */

#include <NiusWireless.h>

#define RC522_CS_PIN    SDA
#define RC522_RST_PIN   10
#define RC522_SCK_PIN   SCL
#define RC522_MOSI_PIN  11
#define RC522_MISO_PIN  12

NiusRC522 rfid(RC522_CS_PIN, RC522_RST_PIN, RC522_SCK_PIN, RC522_MOSI_PIN, RC522_MISO_PIN);

/* ------------------------------------------------------------------ */

/* Map a return code into a short tag for the log. */
static const char *errName(uint8_t r) {
    switch (r) {
        case 0x00: return "OK";
        case 0x01: return "NOTAG";
        case 0x02: return "TIMEOUT";
        case 0x03: return "CRC";
        case 0x04: return "COLLISION";
        case 0x05: return "AUTH";
        case 0x06: return "OVERFLOW";
        case 0x07: return "PARAM";
        case 0xFF: return "UNKNOWN";
        default:   return "?";
    }
}

/* Print 16 bytes as hex. */
static void printHex16(const uint8_t *b) {
    for (uint8_t i = 0; i < 16; i++) {
        if (i) Serial.print(' ');
        if (b[i] < 0x10) Serial.print('0');
        Serial.print(b[i], HEX);
    }
    Serial.println();
}

/* Print 4 bytes as hex. */
static void printHex4(const uint8_t *b) {
    for (uint8_t i = 0; i < 4; i++) {
        if (i) Serial.print(' ');
        if (b[i] < 0x10) Serial.print('0');
        Serial.print(b[i], HEX);
    }
    Serial.println();
}

/* Dump the registers that define the chip's behaviour. Useful when
 * debugging a firmware-vs-card question: if these don't look right,
 * the chip is misconfigured; if they look fine and the card still
 * doesn't respond, the card is the problem. */
static void dumpKeyRegs(NiusRC522 &r) {
    // Read register layout is exposed via getRegister / readRegister.
    // (Header uses MFRC522_REG_* constants.)
    uint8_t v01 = r.readRegister(0x01);  // CommandReg
    uint8_t v09 = r.readRegister(0x09);  // RxThresholdReg (coll-level)
    uint8_t v0A = r.readRegister(0x0A);  // DemodReg
    uint8_t v0C = r.readRegister(0x0C);  // TxASKReg
    uint8_t v0D = r.readRegister(0x0D);  // ModeReg
    uint8_t v11 = r.readRegister(0x11);  // ModeReg (deeper)
    uint8_t v14 = r.readRegister(0x14);  // TxControlReg
    uint8_t v15 = r.readRegister(0x15);  // TxASKReg (newer datasheet 1.x)
    uint8_t v22 = r.readRegister(0x22);  // ModeReg
    uint8_t v2A = r.readRegister(0x2A);  // TModeReg
    uint8_t v2B = r.readRegister(0x2B);  // TPrescalerReg
    uint8_t v2C = r.readRegister(0x2C);  // TReloadReg-Hi
    uint8_t v2D = r.readRegister(0x2D);  // TReloadReg-Lo
    uint8_t v37 = r.readRegister(0x37);  // VersionReg

    Serial.println(F("--- Key registers ---"));
    auto hex2 = [](uint8_t x) {
        Serial.print(F("0x"));
        if (x < 0x10) Serial.print('0');
        Serial.print(x, HEX);
    };
    Serial.print(F("  VersionReg       ")); hex2(v37); Serial.println();
    Serial.print(F("  CommandReg       ")); hex2(v01); Serial.println();
    Serial.print(F("  ModeReg          ")); hex2(v22);
    Serial.print(F("    TxASKReg         ")); hex2(v0C); Serial.println();
    Serial.print(F("  TModeReg         ")); hex2(v2A); Serial.println();
    Serial.print(F("  TPrescalerReg    ")); hex2(v2B); Serial.println();
    Serial.print(F("  TReload          ")); hex2(v2C);
    Serial.print(' '); hex2(v2D); Serial.println();
    Serial.print(F("  TxControlReg     ")); hex2(v14);
    if ((v14 & 0x03) == 0x03) Serial.println(F("  antenna ON"));
    else                      Serial.println(F("  antenna OFF"));
    Serial.println();
}

/* ------------------------------------------------------------------ */

void setup() {
    Serial.begin(9600);
    delay(1500);
    Serial.println(F("NiusWireless — RC522 firmware health check"));
    Serial.println(F("============================================"));

    if (!rfid.begin()) {
        Serial.println(F("FATAL: begin() failed. Reader not detected."));
        Serial.println(F("       Check 3.3V / GND / SPI / RST wiring."));
        while (1) { delay(500); }
    }
    Serial.print(F("Reader: ")); Serial.println(rfid.getVersion());
    Serial.println();

    dumpKeyRegs(rfid);
}

/* ------------------------------------------------------------------ */

/* Dump the registers that contain the result of the last transceive:
 *   ComIrqReg  (0x04)  — interrupt source
 *   ErrorReg   (0x06)  — protocol / buffer / parity errors
 *   Status2Reg (0x08)  — Crypto1On, MFCrypto1On bits
 *   FIFOLevReg (0x0A)  — bytes still in the FIFO
 * Use this to tell *why* a transceive failed. */
static void dumpFailureRegs(NiusRC522 &r) {
    auto hex2 = [](uint8_t x) {
        Serial.print(F("0x"));
        if (x < 0x10) Serial.print('0');
        Serial.print(x, HEX);
    };
    uint8_t irq  = r.readRegister(0x04);  // ComIrqReg
    uint8_t err  = r.readRegister(0x06);  // ErrorReg
    uint8_t s2   = r.readRegister(0x08);  // Status2Reg
    uint8_t fifo = r.readRegister(0x0A);  // FIFOLevelReg
    Serial.print(F("    IRQ=")); hex2(irq);
    Serial.print(F("  ERR=")); hex2(err);
    Serial.print(F("  Status2=")); hex2(s2);
    Serial.print(F("  FIFOLev=")); hex2(fifo);
    Serial.println();
    if (err) {
        if (err & 0x01) Serial.println(F("    - BufferOvfl"));
        if (err & 0x02) Serial.println(F("    - ParityErr"));
        if (err & 0x04) Serial.println(F("    - CollErr"));
        if (err & 0x08) Serial.println(F("    - CollErr (post)"));
        if (err & 0x10) Serial.println(F("    - ProtocolErr"));
        if (err & 0x20) Serial.println(F("    - TempErr"));
        if (err & 0x40) Serial.println(F("    - BufferSizeErr"));
    }
}

/* ------------------------------------------------------------------ */

/* Run a raw transceive of `cmd` (without auto CRC) at low level. */
static uint8_t rawTransceive(NiusRC522 &r, const uint8_t *cmd, uint8_t cmdLen,
                             uint8_t *back, uint8_t *backLen) {
    uint8_t send[cmdLen + 2];
    // Compute CRC via the helper calcCRC exposed privately through executeCommand;
    // we instead use the public API: rfid.transceive takes the bytes WITHOUT
    // auto-CRC because we'll pre-pend the CRC via a separate path. Simpler:
    // use the public transceive() helper which expects the caller's data and
    // appends CRC if checkCRC=true. Return value is the result code.
    return r.transceive((uint8_t*)cmd, cmdLen, back, backLen, nullptr, 0, /*checkCRC*/false);
}

/* ------------------------------------------------------------------ */

void loop() {
    if (!rfid.cardPresent()) {
        static uint32_t last = 0;
        if (millis() - last > 1500) {
            Serial.println(F("[idle] place a card on the reader…"));
            last = millis();
        }
        return;
    }

    Serial.println(F("\n===== Tag detected ====="));
    Serial.print(F("UID   ")); Serial.println(rfid.getUID());
    Serial.print(F("ATQA  ")); Serial.println(rfid.getATQA());
    Serial.print(F("SAK   0x"));
    if (rfid.getSAK() < 0x10) Serial.print('0');
    Serial.println(rfid.getSAK(), HEX);
    Serial.print(F("Type  ")); Serial.println(rfid.getCardTypeName());

    // Re-dump registers on every tag-detection event so register state
    // is observable without timing the boot dump.
    dumpKeyRegs(rfid);

    uint8_t ct = rfid.getCardType();
    Serial.println();

    if (ct == NIUS_CARD_MIFARE_1K || ct == NIUS_CARD_MIFARE_MINI || ct == NIUS_CARD_MIFARE_PLUS) {
        Serial.println(F("[MIFARE Classic roundtrip]"));

        uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        Serial.print(F("  authenticate(4, KEY_A) = "));
        uint8_t rA = rfid.authenticate(4, NIUS_KEY_A, key);
        Serial.println(errName(rA));
        if (rA != 0) dumpFailureRegs(rfid);

        if (rA == 0) {
            uint8_t data[16];
            Serial.print(F("  readBlock(4)            = "));
            uint8_t rR = rfid.readBlock(4, data);
            Serial.println(errName(rR));
            if (rR != 0) dumpFailureRegs(rfid);
            if (rR == 0) {
                Serial.print(F("    data: "));
                printHex16(data);

                // Optional: write a marker byte, read back, restore.
                uint8_t orig = data[0];
                data[0] = (uint8_t)0xAB;
                Serial.print(F("  writeBlock(4, 0xAB)     = "));
                uint8_t rW = rfid.writeBlock(4, data);
                Serial.println(errName(rW));
                if (rW != 0) dumpFailureRegs(rfid);

                if (rW == 0) {
                    uint8_t tmp[16];
                    if (rfid.readBlock(4, tmp) == 0) {
                        Serial.print(F("    read-back: "));
                        printHex16(tmp);
                    }
                    // restore
                    data[0] = orig;
                    Serial.print(F("  writeBlock(4, restore)  = "));
                    Serial.println(errName(rfid.writeBlock(4, data)));
                }
            }
            rfid.stopCrypto();
        }

    } else if (ct == NIUS_CARD_MIFARE_UL) {
        Serial.println(F("[Ultralight roundtrip]"));

        uint8_t v[8];
        uint8_t rV = rfid.getNTAGVersion(v);
        Serial.print(F("  getNTAGVersion         = "));
        Serial.println(errName(rV));
        if (rV != 0) dumpFailureRegs(rfid);
        if (rV == 0) {
            Serial.print(F("    raw: "));
            for (uint8_t i = 0; i < 8; i++) {
                if (i) Serial.print(' ');
                if (v[i] < 0x10) Serial.print('0');
                Serial.print(v[i], HEX);
            }
            Serial.println();
        }

        uint8_t buf[16];
        Serial.print(F("  readPage(0)            = "));
        uint8_t rP0 = rfid.readPage(0, buf);
        Serial.println(errName(rP0));
        if (rP0 != 0) dumpFailureRegs(rfid);
        if (rP0 == 0) { Serial.print(F("    data: ")); printHex16(buf); }

        Serial.print(F("  readPage(4)            = "));
        uint8_t rP4 = rfid.readPage(4, buf);
        Serial.println(errName(rP4));
        if (rP4 != 0) dumpFailureRegs(rfid);
        if (rP4 == 0) { Serial.print(F("    data: ")); printHex16(buf); }

        // --------- Aggressive recovery attempts ----------
        // 1. 30-second antenna-off deep power cycle.
        // 2. Re-select after power cycle.
        // 3. Raw HALT / raw GET_VERSION with checkCRC=false so we can see
        //    whether the chip is timing out because of firmware-side
        //    problems OR returning an actual response the firmware is
        //    mis-classifying as TIMEOUT.

        Serial.println(F("\n[recovery attempts]"));

        // (1) Deep power cycle. 5 s is enough to fully discharge the
        // card-side capacitor and trigger a clean reset of the card's
        // internal state; longer than that just upsets the MFRC522's
        // analog state for no extra benefit on this hardware.
        Serial.println(F("  (1) power cycle: antennaOff + 5 s + antennaOn + reset"));
        rfid.antennaOff();
        for (uint8_t i = 0; i < 5; i++) {
            delay(1000);
        }
        rfid.antennaOn();
        // Counterfeit MFRC522 (VersionReg=0x18) holds stale analog state
        // after long power-downs. Re-soft-reset the chip before resuming
        // so the chip's CommandReg / FIFO state is clean for the next
        // transceive. (`reset()` does a SOFT_RESET + antennaOn.)
        rfid.reset();
        delay(200);

        if (!rfid.cardPresentWake()) {
            Serial.println(F("    card not re-detected after power cycle — re-applied by hand?"));
        } else {
            Serial.println(F("    re-detected after power cycle."));
            uint8_t buf2[16];
            Serial.print(F("    readPage(0)            = "));
            uint8_t rPP = rfid.readPage(0, buf2);
            Serial.println(errName(rPP));
            if (rPP != 0) dumpFailureRegs(rfid);
            if (rPP == 0) { Serial.print(F("      data: ")); printHex16(buf2); }

            // Try several more variants after fresh select.
            Serial.print(F("    readPage(4)            = "));
            Serial.println(errName(rfid.readPage(4, buf2)));

            Serial.print(F("    getNTAGVersion         = "));
            uint8_t v2[8];
            Serial.println(errName(rfid.getNTAGVersion(v2)));
        }

        // (2) Raw transceive with CRC disabled — strips any chance the
        // firmware is timing out because of bad CRC verify logic.
        Serial.println(F("  (2) raw GET_VERSION (0x60) with checkCRC=false"));
        {
            uint8_t rawCmd[1] = { 0x60 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 1, back, &backLen);
            Serial.print(F("    raw GET_VERSION = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
        }

        Serial.println(F("  (3) raw READ page 0 (0x30 0x00) with checkCRC=false"));
        {
            uint8_t rawCmd[2] = { 0x30, 0x00 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 2, back, &backLen);
            Serial.print(F("    raw READ p0    = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
        }

        // (4) Gen1 / Gen2-style magic HALT sequence: 0x40 0x00 (un-halt).
        //     These chips sometimes accept specific raw opcodes that the
        //     stock library won't normally send.
        Serial.println(F("  (4) raw magic 0x40 0x00 (Gen1/Gen2 un-halt)"));
        {
            uint8_t rawCmd[2] = { 0x40, 0x00 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 2, back, &backLen);
            Serial.print(F("    raw 0x40 0x00 = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
            delay(100);
        }

        // (5) Gen2 backdoor seed: 0xF0 0xAA — Fudan recovery prefix.
        Serial.println(F("  (5) raw magic 0xF0 0xAA (Fudan-style prefix)"));
        {
            uint8_t rawCmd[2] = { 0xF0, 0xAA };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 2, back, &backLen);
            Serial.print(F("    raw 0xF0 0xAA = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
            delay(100);
        }

        // (6) NTAG PWD_AUTH with default (all-zero) password.
        //     Some CUID/EV1 chips have been seen with a 0000h default
        //     password; if AUTH bypasses the lock-out, subsequent reads
        //     may work.
        Serial.println(F("  (6) raw PWD_AUTH (0x1A) with all-0 password"));
        {
            uint8_t rawCmd[6] = { 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 6, back, &backLen);
            Serial.print(F("    raw PWD_AUTH (0x00 0x00 0x00 0x00) = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
            delay(100);
        }

        // (7) Same PWD_AUTH but with the all-FF default some chip
        //     revisions use.
        Serial.println(F("  (7) raw PWD_AUTH (0x1A) with all-FF password"));
        {
            uint8_t rawCmd[6] = { 0x1A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 6, back, &backLen);
            Serial.print(F("    raw PWD_AUTH (0xFF 0xFF 0xFF 0xFF) = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
            delay(100);
        }

        // (8) Standard MIFARE HALT (sanity — card should ACK if alive).
        Serial.println(F("  (8) raw HALT (0x50 0x00)"));
        {
            uint8_t rawCmd[2] = { 0x50, 0x00 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 2, back, &backLen);
            Serial.print(F("    raw HALT = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen="));
            Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
            delay(100);
        }

        // (10) Power-cycle + readPage loop — does the card respond fresh
        //      to every command if we tickle the field each time?
        //      Reading several different page addresses to test whether
        //      the brick is page-0-specific.
        Serial.println(F("\n  (10) power-cycle + readPage sweep"));
        for (uint8_t p = 0; p < 16; p += 4) {
            // Re-select + a fresh 200 ms flicker so the card's protocol
            // engine is in its post-reset "first command" state.
            rfid.antennaOff();
            delay(200);
            rfid.antennaOn();
            rfid.reset();
            delay(100);
            if (!rfid.cardPresentWake()) {
                Serial.print(F("    p")); Serial.print(p);
                Serial.println(F(" — re-detect failed, skipping"));
                continue;
            }
            uint8_t buf[16];
            uint8_t r = rfid.readPage(p, buf);
            Serial.print(F("    p")); Serial.print(p);
            Serial.print(F(" = "));
            Serial.print(errName(r));
            if (r == 0) {
                Serial.print(F("  data: "));
                printHex16(buf);
            } else if (r != 0) {
                Serial.print(F("  IRQ=0x")); Serial.print(rfid.readRegister(0x04), HEX);
                Serial.print(F(" ERR=0x")); Serial.print(rfid.readRegister(0x06), HEX);
            }
            Serial.println();
        }

        // (11) Single power cycle, then chain several commands back-to-back
        //      without re-cycling. The first returns AUTH; do the others
        //      also return AUTH, or do they time out? This tells us whether
        //      the protocol engine stays alive across multiple commands.
        Serial.println(F("\n  (11) single power cycle, chained commands"));
        rfid.antennaOff();
        delay(200);
        rfid.antennaOn();
        rfid.reset();
        delay(100);
        if (!rfid.cardPresentWake()) {
            Serial.println(F("    re-detect failed"));
        } else {
            for (uint8_t cmd = 0; cmd < 4; cmd++) {
                uint8_t buf[16];
                uint8_t r = rfid.readPage((cmd & 1) ? 4 : 0, buf);
                Serial.print(F("    chained read #")); Serial.print(cmd);
                Serial.print(F(": "));
                Serial.println(errName(r));
            }
        }

        // (12) Single power cycle, then MIFARE_CMD_WRITE (0xA0) attempts.
        //      Some CUID chips respond to 0xA0 even when they should be
        //      Ultralight (under the CUID backdoor).
        Serial.println(F("\n  (12) MIFARE_CMD_WRITE (0xA0) attempts after cycle"));
        rfid.antennaOff();
        delay(200);
        rfid.antennaOn();
        rfid.reset();
        delay(100);
        if (!rfid.cardPresentWake()) {
            Serial.println(F("    re-detect failed"));
        } else {
            // 0xA0 = MIFARE Classic WRITE. Send an address-only frame
            // (without a 16-byte payload, just the WRITE prefix) — it
            // will either trigger an AUTH flow or a NAK.
            uint8_t rawCmd[2] = { 0xA0, 0x00 };
            uint8_t back[16];
            uint8_t backLen = sizeof(back);
            uint8_t rr = rawTransceive(rfid, rawCmd, 2, back, &backLen);
            Serial.print(F("    raw 0xA0 0x00 = "));
            Serial.print(errName(rr));
            Serial.print(F("  backLen=")); Serial.println(backLen);
            if (rr != 0) dumpFailureRegs(rfid);
        }

    } else {
        Serial.println(F("[unknown card type — running GET_VERSION + readPage(0) probe]"));

        uint8_t v[8];
        uint8_t rV = rfid.getNTAGVersion(v);
        Serial.print(F("  getNTAGVersion         = "));
        Serial.println(errName(rV));
        if (rV != 0) dumpFailureRegs(rfid);

        uint8_t buf[16];
        Serial.print(F("  readPage(0)            = "));
        uint8_t rP0 = rfid.readPage(0, buf);
        Serial.println(errName(rP0));
        if (rP0 != 0) dumpFailureRegs(rfid);
        if (rP0 == 0) { Serial.print(F("    data: ")); printHex16(buf); }
    }

    rfid.halt();
    uint32_t ws = millis();
    while ((millis() - ws) < 2000UL) {
        if (!rfid.cardPresentWake()) break;
        rfid.halt();
        delay(150);
    }
}
