/*
 * NiusRC522.cpp — Full MFRC522 driver implementation
 *
 * SPI protocol: Mode 0 (CPOL=0, CPHA=0), MSB first.
 * Register address byte: bit7=R/W (1=read,0=write), bits[6:1]=address, bit0=0.
 */

#include "NiusRC522.h"

/* -----------------------------------------------------------------------
 * Default MIFARE Classic key (all 0xFF)
 * ---------------------------------------------------------------------- */
const uint8_t NIUS_KEY_DEFAULT[NIUS_KEY_DEFAULT_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* =======================================================================
 * Constructors
 * ====================================================================== */

NiusRC522::NiusRC522(uint8_t csPin, uint8_t rstPin) {
    _csPin    = csPin;
    _rstPin   = rstPin;
    _sckPin   = 0;
    _mosiPin  = 0;
    _misoPin  = 0;
    _irqPin   = 0xFF;  // 0xFF = not assigned
    _softSPI  = false;
    _spiSpeed = 4000000UL;
    _ready    = false;
    uidLen    = 0;
    lastCardType    = NIUS_CARD_UNKNOWN;
    lastError       = NIUS_OK;
    lastSelectError = 0xFF;  // 0xFF = selectCard never called
    memset(uid, 0, sizeof(uid));
}

NiusRC522::NiusRC522(uint8_t csPin, uint8_t rstPin,
                     uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin) {
    _csPin    = csPin;
    _rstPin   = rstPin;
    _sckPin   = sckPin;
    _mosiPin  = mosiPin;
    _misoPin  = misoPin;
    _irqPin   = 0xFF;
    _softSPI  = true;
    _spiSpeed = 0;
    _ready    = false;
    uidLen    = 0;
    lastCardType    = NIUS_CARD_UNKNOWN;
    lastError       = NIUS_OK;
    lastSelectError = 0xFF;
    memset(uid, 0, sizeof(uid));
}

/* =======================================================================
 * NiusBase interface
 * ====================================================================== */

bool NiusRC522::begin() {
    return begin(4000000UL);
}

bool NiusRC522::begin(uint32_t spiSpeed) {
    _spiSpeed = spiSpeed;

    /* ---- GPIO setup -------------------------------------------------- */
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);

    if (_softSPI) {
        pinMode(_sckPin, OUTPUT);
        digitalWrite(_sckPin, LOW);
        pinMode(_mosiPin, OUTPUT);
        digitalWrite(_mosiPin, LOW);
        pinMode(_misoPin, INPUT);
    } else {
        SPI.begin();
    }

    /* ---- Hardware reset ---------------------------------------------- */
    // Bring RST HIGH for at least 100 ns, then the chip auto-starts.
    // We hold it LOW for 50 ms then release.
    delay(50);
    digitalWrite(_rstPin, HIGH);
    delay(50);  // Give the oscillator time to start up (typ. 37.74 µs)

    /* ---- Soft reset --------------------------------------------------- */
    writeRegister(MFRC522_REG_COMMAND, MFRC522_CMD_SOFT_RESET);
    // Poll until PowerDown bit (bit 4 of CommandReg) clears
    uint8_t count = 0;
    while (count < 50) {
        if ((readRegister(MFRC522_REG_COMMAND) & 0x10) == 0) { break; }
        delay(10);
        count++;
    }
    if (count >= 50) { return false; }

    /* ---- Timer: auto restart, ~25 ms timeout ------------------------- */
    // f_timer = 13.56 MHz / (2 * (TPrescaler+1)) = 40 kHz with 0xA9
    // Reload = 1000 → period = 1000 / 40000 = 25 ms
    writeRegister(MFRC522_REG_T_MODE,      0x80); // TAuto=1
    writeRegister(MFRC522_REG_T_PRESCALER, 0xA9);
    writeRegister(MFRC522_REG_T_RELOAD_H,  0x03);
    writeRegister(MFRC522_REG_T_RELOAD_L,  0xE8);

    /* ---- Modulation width and force 100 % ASK ------------------------ */
    writeRegister(MFRC522_REG_TX_ASK, 0x40);

    /* ---- General mode: CRC preset 0x6363 (ISO 14443 CRC_A) ---------- */
    writeRegister(MFRC522_REG_MODE, 0x3D);

    /* ---- Turn antenna on --------------------------------------------- */
    antennaOn();

    /* ---- Verify chip is present -------------------------------------- */
    uint8_t ver = readRegister(MFRC522_REG_VERSION);
    if (ver == 0x00 || ver == 0xFF) { return false; }

    _ready = true;
    return true;
}

bool NiusRC522::isReady() {
    if (!_ready) { return false; }
    uint8_t ver = readRegister(MFRC522_REG_VERSION);
    return (ver != 0x00 && ver != 0xFF);
}

void NiusRC522::reset() {
    writeRegister(MFRC522_REG_COMMAND, MFRC522_CMD_SOFT_RESET);
    uint8_t count = 0;
    while (count < 50) {
        if ((readRegister(MFRC522_REG_COMMAND) & 0x10) == 0) { break; }
        delay(10);
        count++;
    }
    antennaOn();
}

String NiusRC522::getVersion() {
    uint8_t ver = readRegister(MFRC522_REG_VERSION);
    switch (ver) {
        case MFRC522_VERSION_1_0:    return "MFRC522 v1.0";
        case MFRC522_VERSION_2_0:    return "MFRC522 v2.0";
        case MFRC522_VERSION_FM17522: return "FM17522 (Fudan)";
        case MFRC522_VERSION_FM17522E:return "FM17522E (Fudan)";
        default: {
            String s = "MFRC522 raw=0x";
            if (ver < 0x10) { s += '0'; }
            s += String(ver, HEX);
            return s;
        }
    }
}

/* =======================================================================
 * Card detection
 * ====================================================================== */

bool NiusRC522::cardPresent() {
    lastSelectError = 0xFF;
    uint8_t r = requestA(this->atqa);
    if (r != NIUS_OK) { lastError = r; return false; }
    uint8_t outUID[NIUS_UID_MAX_LEN];
    uint8_t outLen = 0;
    uint8_t outSAK = 0;
    r = selectCard(outUID, &outLen, &outSAK);
    lastSelectError = r;
    if (r != NIUS_OK) { lastError = r; return false; }
    memcpy(uid, outUID, outLen);
    uidLen = outLen;
    sak = outSAK;
    lastCardType = sakToCardType(outSAK);
    lastError = NIUS_OK;
    return true;
}

bool NiusRC522::cardPresentWake() {
    lastSelectError = 0xFF;
    uint8_t r = wakeupA(this->atqa);
    if (r != NIUS_OK) { lastError = r; return false; }
    uint8_t outUID[NIUS_UID_MAX_LEN];
    uint8_t outLen = 0;
    uint8_t outSAK = 0;
    r = selectCard(outUID, &outLen, &outSAK);
    lastSelectError = r;
    if (r != NIUS_OK) { lastError = r; return false; }
    memcpy(uid, outUID, outLen);
    uidLen = outLen;
    sak = outSAK;
    lastCardType = sakToCardType(outSAK);
    lastError = NIUS_OK;
    return true;
}

bool NiusRC522::getUIDBytes(uint8_t *buf, uint8_t &len) {
    if (uidLen == 0) { return false; }
    memcpy(buf, uid, uidLen);
    len = uidLen;
    return true;
}

String NiusRC522::getUID() {
    if (uidLen == 0) { return ""; }
    String s = "";
    for (uint8_t i = 0; i < uidLen; i++) {
        if (uid[i] < 0x10) { s += '0'; }
        s += String(uid[i], HEX);
    }
    s.toUpperCase();
    return s;
}

uint8_t NiusRC522::getCardType() {
    return lastCardType;
}

String NiusRC522::getCardTypeName() {
    switch (lastCardType) {
        case NIUS_CARD_MIFARE_MINI:  return "MIFARE Mini";
        case NIUS_CARD_MIFARE_1K:    return "MIFARE Classic 1K";
        case NIUS_CARD_MIFARE_4K:    return "MIFARE Classic 4K";
        case NIUS_CARD_MIFARE_UL:    return "MIFARE Ultralight";
        case NIUS_CARD_MIFARE_PLUS:  return "MIFARE Plus";
        case NIUS_CARD_ISO14443_4:   return "ISO 14443-4";
        case NIUS_CARD_ISO18092:     return "ISO 18092 (NFC-IP1)";
        case NIUS_CARD_TNP3XXX:      return "TNP3xxx";
        case NIUS_CARD_DESFIRE:      return "MIFARE DESFire";
        default:                      return "Unknown";
    }
}

String NiusRC522::getATQA() {
    if (lastError != NIUS_OK) { return ""; }
    String s = "";
    for (uint8_t i = 0; i < 2; i++) {
        if (atqa[i] < 0x10) { s += '0'; }
        s += String(atqa[i], HEX);
    }
    s.toUpperCase();
    return s;
}

bool NiusRC522::getATQABytes(uint8_t *buf) {
    if (lastError != NIUS_OK) { return false; }
    memcpy(buf, atqa, 2);
    return true;
}

uint8_t NiusRC522::getSAK() {
    return sak;
}

uint8_t NiusRC522::getNTAGVersion(uint8_t *version) {
    // GET_VERSION (0x60) for NTAG / MIFARE Ultralight EV1.
    // Command: 0x60 + 2 CRC bytes. Response: 8 version bytes + 2 CRC.
    uint8_t cmd[3];
    cmd[0] = 0x60;
    uint8_t crc[2];
    uint8_t res = calcCRC(cmd, 1, crc);
    if (res != NIUS_OK) { return res; }
    cmd[1] = crc[0];
    cmd[2] = crc[1];

    uint8_t backData[10];
    uint8_t backLen = sizeof(backData);
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         cmd, 3, backData, &backLen, nullptr, 0, true);
    if (res != NIUS_OK) { return res; }
    if (backLen < 8) { return NIUS_ERR_UNKNOWN; }
    memcpy(version, backData, 8);
    return NIUS_OK;
}

void NiusRC522::halt() {
    uint8_t buf[4];
    buf[0] = MIFARE_CMD_HALT_MSB;
    buf[1] = MIFARE_CMD_HALT_LSB;
    uint8_t crc[2];
    if (calcCRC(buf, 2, crc) != NIUS_OK) { return; }
    buf[2] = crc[0];
    buf[3] = crc[1];
    // Send HALT — we do not care about the response (card goes silent)
    executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30, buf, 4, nullptr, nullptr, nullptr, 0, false);
    // Clear MFCrypto1On bit so we can detect new cards
    clearRegisterBits(MFRC522_REG_STATUS2, 0x08);
}

/* =======================================================================
 * MIFARE Classic operations
 * ====================================================================== */

uint8_t NiusRC522::authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key) {
    // Build authentication command buffer:
    // [0]=keyType, [1]=blockAddr, [2..7]=key, [8..11]=4 UID bytes
    uint8_t buf[12];
    buf[0] = keyType;
    buf[1] = blockAddr;
    for (uint8_t i = 0; i < 6; i++) { buf[2 + i] = key[i]; }
    for (uint8_t i = 0; i < 4; i++) { buf[8 + i] = uid[i]; }

    uint8_t waitIRQ = 0x10;  // IdleIRq bit
    uint8_t result = executeCommand(MFRC522_CMD_MF_AUTHENT, waitIRQ,
                                    buf, 12, nullptr, nullptr, nullptr, 0, false);
    if (result != NIUS_OK) { return NIUS_ERR_AUTH; }

    // Check that crypto1 was activated (MFCrypto1On bit set)
    if (!(readRegister(MFRC522_REG_STATUS2) & 0x08)) { return NIUS_ERR_AUTH; }
    return NIUS_OK;
}

uint8_t NiusRC522::readBlock(uint8_t blockAddr, uint8_t *data) {
    uint8_t cmd[4];
    cmd[0] = MIFARE_CMD_READ;
    cmd[1] = blockAddr;
    uint8_t crc[2];
    uint8_t res = calcCRC(cmd, 2, crc);
    if (res != NIUS_OK) { return res; }
    cmd[2] = crc[0];
    cmd[3] = crc[1];

    uint8_t backData[18];
    uint8_t backLen = sizeof(backData);
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         cmd, 4, backData, &backLen, nullptr, 0, true);
    if (res != NIUS_OK) { return res; }
    // MIFARE Classic READ returns 16 data bytes followed by 2 CRC bytes.
    // executeCommand() verifies the CRC and returns OK with backLen == 18
    // for a valid response. Accept 16 (CRC stripped) or 18 (with CRC) and
    // copy the first 16 bytes — the 2 trailing bytes, if present, are CRC.
    if (backLen < 16) { return NIUS_ERR_UNKNOWN; }
    memcpy(data, backData, 16);
    return NIUS_OK;
}

uint8_t NiusRC522::writeBlock(uint8_t blockAddr, uint8_t *data) {
    // Phase 1: send WRITE command + block address
    uint8_t cmd[4];
    cmd[0] = MIFARE_CMD_WRITE;
    cmd[1] = blockAddr;
    uint8_t crc[2];
    uint8_t res = calcCRC(cmd, 2, crc);
    if (res != NIUS_OK) { return res; }
    cmd[2] = crc[0];
    cmd[3] = crc[1];

    uint8_t ack[1];
    uint8_t ackLen = 1;
    uint8_t validBits = 0;
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         cmd, 4, ack, &ackLen, &validBits, 0, false);
    if (res != NIUS_OK) { return res; }
    if (ackLen != 1 || validBits != 4) { return NIUS_ERR_UNKNOWN; }
    if ((ack[0] & 0x0F) != MIFARE_ACK) { return NIUS_ERR_AUTH; }

    // Phase 2: send 16 bytes of data + CRC
    uint8_t buf[18];
    memcpy(buf, data, 16);
    res = calcCRC(buf, 16, crc);
    if (res != NIUS_OK) { return res; }
    buf[16] = crc[0];
    buf[17] = crc[1];

    ackLen = 1;
    validBits = 0;
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         buf, 18, ack, &ackLen, &validBits, 0, false);
    if (res != NIUS_OK) { return res; }
    if (ackLen != 1 || validBits != 4) { return NIUS_ERR_UNKNOWN; }
    if ((ack[0] & 0x0F) != MIFARE_ACK) { return NIUS_ERR_AUTH; }
    return NIUS_OK;
}

void NiusRC522::stopCrypto() {
    clearRegisterBits(MFRC522_REG_STATUS2, 0x08);  // Clear MFCrypto1On
}

/* =======================================================================
 * High-level helpers — modelled on the standard MFRC522 library's
 * MIFARE_SetUid / PICC_DumpToSerial, so sketches can do the natural
 * "dump it / change the UID" thing in a single call.
 * ====================================================================== */

uint8_t NiusRC522::setUid(uint8_t *newUid, uint8_t uidSize) {
    if (!newUid || uidSize == 0 || uidSize > 15) { return NIUS_ERR_PARAM; }

    // Build the new block 0:  [UID bytes] [BCC] [12 bytes of zeros]
    // For a 4-byte UID (MIFARE Classic 1K / Mini) this is the standard layout.
    uint8_t factoryKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t block0[16] = {0};
    memcpy(block0, newUid, uidSize);
    for (uint8_t i = 0; i < uidSize; i++) { block0[uidSize] ^= newUid[i]; }  // BCC
    // Bytes uidSize+1..15 left at 0 (manufacturer data — CUID cards accept zeros)

    if (authenticate(3, NIUS_KEY_A, factoryKey) != NIUS_OK) {
        if (authenticate(3, NIUS_KEY_B, factoryKey) != NIUS_OK) {
            return NIUS_ERR_AUTH;
        }
    }
    uint8_t r = writeBlock(0, block0);
    stopCrypto();
    return r;
}

uint8_t NiusRC522::dumpClassic(uint8_t *key, void (*printer)(uint8_t *)) {
    uint8_t okSectors = 0;
    for (uint8_t s = 0; s < 16; s++) {
        uint8_t trailer = s * 4 + 3;
        if (authenticate(trailer, NIUS_KEY_A, key) != NIUS_OK) {
            stopCrypto();
            continue;
        }
        okSectors++;
        for (uint8_t b = 0; b < 4; b++) {
            uint8_t buf[16];
            if (readBlock(s * 4 + b, buf) == NIUS_OK && printer) {
                printer(buf);
            }
        }
        stopCrypto();
    }
    return okSectors;
}

uint8_t NiusRC522::dumpUltralight(void (*printer)(uint8_t *)) {
    uint8_t pages = 0;
    for (uint8_t p = 0; p < 64; p += 4) {
        uint8_t buf[16];
        if (readPage(p, buf) != NIUS_OK) { break; }
        pages += 4;
        if (printer) { printer(buf); }
    }
    return pages;
}

/* =======================================================================
 * MIFARE Ultralight / NTAG operations
 * No authentication. Page-addressed. READ returns 4 pages (16 bytes)
 * at a time, WRITE writes a single 4-byte page.
 * ====================================================================== */

uint8_t NiusRC522::readPage(uint8_t page, uint8_t *data) {
    uint8_t cmd[4];
    cmd[0] = MIFARE_CMD_READ;        // 0x30 (same code as Classic)
    cmd[1] = page;
    uint8_t crc[2];
    uint8_t res = calcCRC(cmd, 2, crc);
    if (res != NIUS_OK) { return res; }
    cmd[2] = crc[0];
    cmd[3] = crc[1];

    uint8_t backData[18];
    uint8_t backLen = sizeof(backData);
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         cmd, 4, backData, &backLen, nullptr, 0, true);
    if (res != NIUS_OK) { return res; }
    // Ultralight/NTAG READ returns 4 pages (16 bytes) + 2 CRC bytes.
    if (backLen < 16) { return NIUS_ERR_UNKNOWN; }
    memcpy(data, backData, 16);
    return NIUS_OK;
}

uint8_t NiusRC522::writePage(uint8_t page, uint8_t *data) {
    // Ultralight WRITE: 0xA2 + page + 4 data bytes + 2 CRC bytes
    uint8_t cmd[8];
    cmd[0] = MIFARE_CMD_WRITE_UL;    // 0xA2
    cmd[1] = page;
    memcpy(&cmd[2], data, 4);
    uint8_t crc[2];
    uint8_t res = calcCRC(cmd, 6, crc);
    if (res != NIUS_OK) { return res; }
    cmd[6] = crc[0];
    cmd[7] = crc[1];

    // The card responds with a 4-bit ACK (0xA) on success or NAK.
    uint8_t ack[1];
    uint8_t ackLen = 1;
    uint8_t validBits = 0;
    res = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                         cmd, 8, ack, &ackLen, &validBits, 0, false);
    if (res != NIUS_OK) { return res; }
    if (ackLen != 1 || validBits != 4) { return NIUS_ERR_UNKNOWN; }
    if ((ack[0] & 0x0F) != MIFARE_ACK) { return NIUS_ERR_AUTH; }
    return NIUS_OK;
}

/* =======================================================================
 * RF / Antenna control
 * ====================================================================== */

void NiusRC522::antennaOn() {
    uint8_t val = readRegister(MFRC522_REG_TX_CONTROL);
    if ((val & 0x03) != 0x03) {
        writeRegister(MFRC522_REG_TX_CONTROL, val | 0x03);
    }
}

void NiusRC522::antennaOff() {
    clearRegisterBits(MFRC522_REG_TX_CONTROL, 0x03);
}

void NiusRC522::setAntennaGain(uint8_t gain) {
    // Gain occupies bits [6:4] of RFCfgReg
    uint8_t val = readRegister(MFRC522_REG_RF_CFG);
    val &= ~0x70;
    val |= (gain & 0x70);
    writeRegister(MFRC522_REG_RF_CFG, val);
}

uint8_t NiusRC522::getAntennaGain() {
    return readRegister(MFRC522_REG_RF_CFG) & 0x70;
}

/* =======================================================================
 * IRQ pin
 * ====================================================================== */

void NiusRC522::setIRQPin(uint8_t irqPin) {
    _irqPin = irqPin;
    pinMode(_irqPin, INPUT);
    // Enable IRQ output: ComIEnReg bit7 (IRqInv=0 means active LOW)
    writeRegister(MFRC522_REG_COM_I_EN, 0xA0);  // IRqInv=1 (active LOW), RxIEn=1
}

/* =======================================================================
 * Raw register access
 * ====================================================================== */

uint8_t NiusRC522::readRegister(uint8_t addr) {
    uint8_t val;
    csLow();
    spiTransfer(0x80 | ((addr << 1) & 0x7E));
    val = spiTransfer(0x00);
    csHigh();
    return val;
}

void NiusRC522::writeRegister(uint8_t addr, uint8_t value) {
    csLow();
    spiTransfer((addr << 1) & 0x7E);
    spiTransfer(value);
    csHigh();
}

void NiusRC522::setRegisterBits(uint8_t addr, uint8_t mask) {
    writeRegister(addr, readRegister(addr) | mask);
}

void NiusRC522::clearRegisterBits(uint8_t addr, uint8_t mask) {
    writeRegister(addr, readRegister(addr) & (~mask));
}

/* =======================================================================
 * Private — SPI helpers
 * ====================================================================== */

void NiusRC522::csLow()  { digitalWrite(_csPin, LOW);  }
void NiusRC522::csHigh() { digitalWrite(_csPin, HIGH); }

uint8_t NiusRC522::spiTransfer(uint8_t data) {
    if (_softSPI) { return softTransfer(data); }

    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    uint8_t ret = SPI.transfer(data);
    SPI.endTransaction();
    return ret;
}

/*
 * softTransfer() — bit-bang SPI Mode 0 (CPOL=0, CPHA=0).
 * Clock idles LOW. Data is set on the falling edge and sampled on the rising edge.
 */
uint8_t NiusRC522::softTransfer(uint8_t data) {
    uint8_t result = 0;
    for (int8_t i = 7; i >= 0; i--) {
        // Set MOSI before rising edge
        digitalWrite(_mosiPin, (data >> i) & 0x01);
        delayMicroseconds(1);
        // Rising edge — sample MISO
        digitalWrite(_sckPin, HIGH);
        if (digitalRead(_misoPin)) { result |= (1 << i); }
        delayMicroseconds(1);
        // Falling edge
        digitalWrite(_sckPin, LOW);
    }
    return result;
}

/* =======================================================================
 * Private — Burst register I/O
 * ====================================================================== */

void NiusRC522::readRegisterBurst(uint8_t addr, uint8_t count,
                                   uint8_t *values, uint8_t rxAlign) {
    if (count == 0) { return; }
    uint8_t addrByte = 0x80 | ((addr << 1) & 0x7E);
    uint8_t index = 0;
    count--;

    csLow();
    spiTransfer(addrByte);

    if (rxAlign) {
        uint8_t mask = (uint8_t)(0xFF << rxAlign);
        uint8_t val = spiTransfer(addrByte);
        values[0] = (values[0] & ~mask) | (val & mask);
        index++;
    }
    while (index < count) {
        values[index] = spiTransfer(addrByte);
        index++;
    }
    values[index] = spiTransfer(0x00);  // Last read — stop burst
    csHigh();
}

void NiusRC522::writeRegisterBurst(uint8_t addr, uint8_t count, uint8_t *values) {
    csLow();
    spiTransfer((addr << 1) & 0x7E);
    for (uint8_t i = 0; i < count; i++) { spiTransfer(values[i]); }
    csHigh();
}

/* =======================================================================
 * Private — MFRC522 executeCommand
 * ====================================================================== */

uint8_t NiusRC522::executeCommand(uint8_t cmd,
                                   uint8_t waitIRQ,
                                   uint8_t *sendData, uint8_t sendLen,
                                   uint8_t *backData, uint8_t *backLen,
                                   uint8_t *validBits,
                                   uint8_t rxAlign,
                                   bool checkCRC) {
    uint8_t txLastBits = validBits ? *validBits : 0;
    uint8_t bitFraming = (rxAlign << 4) | txLastBits;

    writeRegister(MFRC522_REG_COMMAND,     MFRC522_CMD_IDLE);
    writeRegister(MFRC522_REG_COM_IRQ,     0x7F);          // Clear all IRQ bits
    setRegisterBits(MFRC522_REG_FIFO_LEVEL, 0x80);         // Flush FIFO
    writeRegisterBurst(MFRC522_REG_FIFO_DATA, sendLen, sendData);
    writeRegister(MFRC522_REG_BIT_FRAMING, bitFraming);
    writeRegister(MFRC522_REG_COMMAND,     cmd);

    if (cmd == MFRC522_CMD_TRANSCEIVE) {
        setRegisterBits(MFRC522_REG_BIT_FRAMING, 0x80);  // StartSend=1
    }

    // Wait for command to complete (poll ComIrqReg)
    uint16_t timeout = 2000;
    bool completed = false;
    while (timeout--) {
        uint8_t irq = readRegister(MFRC522_REG_COM_IRQ);
        if (irq & waitIRQ)   { completed = true; break; }
        if (irq & 0x01)      { break; }  // Timer interrupt
        delayMicroseconds(10);
    }

    clearRegisterBits(MFRC522_REG_BIT_FRAMING, 0x80);  // Stop StartSend

    if (!completed) { return NIUS_ERR_TIMEOUT; }

    // Check for errors
    uint8_t errReg = readRegister(MFRC522_REG_ERROR);
    if (errReg & 0x13) {  // BufferOvfl, ParityErr, ProtocolErr
        return NIUS_ERR_UNKNOWN;
    }
    if (errReg & 0x04) { return NIUS_ERR_COLLISION; }

    uint8_t _validBits = 0;
    if (backData && backLen) {
        uint8_t n = readRegister(MFRC522_REG_FIFO_LEVEL);
        if (n > *backLen) { return NIUS_ERR_OVERFLOW; }
        *backLen = n;
        readRegisterBurst(MFRC522_REG_FIFO_DATA, n, backData, rxAlign);
        _validBits = readRegister(MFRC522_REG_CONTROL) & 0x07;
        if (validBits) { *validBits = _validBits; }
    }

    if (errReg & 0x08) { return NIUS_ERR_COLLISION; }  // CollErr after read

    if (checkCRC && backData && backLen && *backLen > 0) {
        if (*backLen == 1 && _validBits == 4) { return NIUS_ERR_AUTH; }
        if (*backLen < 2 || _validBits != 0)  { return NIUS_ERR_CRC; }
        if (!verifyCRC(backData, *backLen)) { return NIUS_ERR_CRC; }
    }
    return NIUS_OK;
}

/* =======================================================================
 * Private — CRC helpers
 * ====================================================================== */

uint8_t NiusRC522::calcCRC(uint8_t *data, uint8_t len, uint8_t *result) {
    writeRegister(MFRC522_REG_COMMAND,  MFRC522_CMD_IDLE);
    clearRegisterBits(MFRC522_REG_DIV_IRQ, 0x04);  // Clear CRCIRq
    setRegisterBits(MFRC522_REG_FIFO_LEVEL, 0x80); // Flush FIFO
    writeRegisterBurst(MFRC522_REG_FIFO_DATA, len, data);
    writeRegister(MFRC522_REG_COMMAND,  MFRC522_CMD_CALC_CRC);

    uint16_t timeout = 5000;
    while (timeout--) {
        if (readRegister(MFRC522_REG_DIV_IRQ) & 0x04) { break; }
        delayMicroseconds(1);
    }
    if (timeout == 0) { return NIUS_ERR_TIMEOUT; }

    writeRegister(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    result[0] = readRegister(MFRC522_REG_CRC_RESULT_L);
    result[1] = readRegister(MFRC522_REG_CRC_RESULT_H);
    return NIUS_OK;
}

bool NiusRC522::verifyCRC(uint8_t *data, uint8_t len) {
    if (len < 2) { return false; }
    uint8_t expected[2];
    if (calcCRC(data, len - 2, expected) != NIUS_OK) { return false; }
    return (expected[0] == data[len - 2] && expected[1] == data[len - 1]);
}

/* =======================================================================
 * Private — ISO 14443A protocol
 * ====================================================================== */

uint8_t NiusRC522::requestA(uint8_t *atqa) {
    return requestOrWakeup(MIFARE_CMD_REQA, atqa);
}

uint8_t NiusRC522::wakeupA(uint8_t *atqa) {
    return requestOrWakeup(MIFARE_CMD_WUPA, atqa);
}

uint8_t NiusRC522::requestOrWakeup(uint8_t cmd, uint8_t *atqa) {
    // REQA/WUPA are short frames: 7 data bits, no parity
    clearRegisterBits(MFRC522_REG_COLL, 0x80);  // ValuesAfterColl=0
    uint8_t validBits = 7;
    uint8_t backLen   = 2;
    uint8_t result = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                                    &cmd, 1, atqa, &backLen, &validBits, 0, false);
    if (result != NIUS_OK)  { return result; }
    if (backLen != 2 || validBits != 0) { return NIUS_ERR_UNKNOWN; }
    return NIUS_OK;
}

/*
 * selectCard() — Full ISO 14443A anti-collision + select sequence.
 *
 * Handles single (4-byte), double (7-byte) and triple (10-byte) UIDs and
 * properly resolves bit collisions when multiple cards are in the field by
 * retrying with progressively more known UID bits.
 *
 * outUID  — at least NIUS_UID_MAX_LEN (10) bytes
 * outLen  — set to the final UID byte count
 * outSAK  — set to the Select Acknowledge byte
 */
uint8_t NiusRC522::selectCard(uint8_t *outUID, uint8_t *outLen, uint8_t *outSAK) {
    uint8_t uidIdx    = 0;
    uint8_t result;
    bool uidComplete  = false;

    static const uint8_t cascadeCmd[3] = {
        MIFARE_CASCADE_1, MIFARE_CASCADE_2, MIFARE_CASCADE_3
    };

    for (uint8_t cascade = 0; cascade < 3 && !uidComplete; cascade++) {

        uint8_t lvlUID[4];    // Resolved UID bytes for this cascade level
        uint8_t knownBits = 0;// How many UID bits have been resolved so far

        /* ---- Anti-collision loop (ISO 14443-3 §6.4.2) ------------------- */
        while (knownBits < 32) {

            uint8_t txLastBits  = knownBits % 8;
            uint8_t knownBytes  = knownBits / 8;
            /* NVB (Number of Valid Bits): upper nibble = whole bytes sent
             * (including the 2 cmd bytes), lower nibble = remaining bits.  */
            uint8_t nvb = (uint8_t)(((knownBytes + 2) << 4) | txLastBits);

            uint8_t buf[9];
            buf[0] = cascadeCmd[cascade];
            buf[1] = nvb;
            for (uint8_t i = 0; i < knownBytes; i++) { buf[2 + i] = lvlUID[i]; }
            /* If txLastBits > 0 the last known byte has only txLastBits
             * valid bits — it was already placed in lvlUID. */
            uint8_t sendLen = 2 + knownBytes + (txLastBits > 0 ? 1 : 0);

            clearRegisterBits(MFRC522_REG_COLL, 0x80); // ValuesAfterColl = 0

            /* Pre-fill the receive buffer so that readRegisterBurst can
             * correctly merge received bits into the known-bits portion.
             * When txLastBits > 0, the first receive byte partially overlaps
             * with the last transmitted byte; its lower txLastBits bits must
             * already contain the known bits before the merge. */
            uint8_t backData[5];
            memset(backData, 0, sizeof(backData));
            if (txLastBits > 0) {
                backData[knownBytes] = lvlUID[knownBytes];
            }
            uint8_t backLen   = sizeof(backData);
            uint8_t validBits = txLastBits;

            result = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                                    buf, sendLen, backData, &backLen,
                                    &validBits, txLastBits, false);

            if (result == NIUS_ERR_COLLISION) {
                uint8_t collReg = readRegister(MFRC522_REG_COLL);
                /* CollPosNotValid (bit5) set → could not locate collision */
                if (collReg & 0x20) { return NIUS_ERR_COLLISION; }
                uint8_t collPos = collReg & 0x1F;
                if (collPos == 0) { collPos = 32; }
                if (collPos <= knownBits) { return NIUS_ERR_COLLISION; }

                /* Merge the received bytes into lvlUID up to the collision
                 * bit, then force the colliding bit to 1 to select one card */
                for (uint8_t i = 0; i < 4; i++) { lvlUID[i] = backData[i]; }
                uint8_t colByte = (collPos - 1) >> 3;
                uint8_t colBit  = (collPos - 1) & 7;
                lvlUID[colByte] |= (uint8_t)(1u << colBit);
                knownBits = collPos;

            } else if (result != NIUS_OK) {
                return result;

            } else {
                /* No collision — received all remaining UID bits */
                /* Merge received bits into lvlUID */
                for (uint8_t i = 0; i < backLen; i++) {
                    if (i < 4) { lvlUID[i] = backData[i]; }
                }
                knownBits = 32;
            }
        } /* while knownBits < 32 */

        /* Verify BCC (5th received byte = backData[4] from last exchange) */
        uint8_t bcc = lvlUID[0] ^ lvlUID[1] ^ lvlUID[2] ^ lvlUID[3];
        /* Note: backData[4] is the BCC from the last anti-collision exchange.
         * We skip the BCC check here to stay tolerant of noisy fields;
         * a failed SELECT CRC below catches real errors. */

        /* Copy cascade-level UID bytes into outUID (skip cascade tag 0x88) */
        uint8_t copyStart = (lvlUID[0] == MIFARE_CT) ? 1 : 0;
        for (uint8_t i = copyStart; i < 4; i++) {
            if (uidIdx < NIUS_UID_MAX_LEN) { outUID[uidIdx++] = lvlUID[i]; }
        }

        /* ---- SELECT ------------------------------------------------------ */
        uint8_t sel[9];
        sel[0] = cascadeCmd[cascade];
        sel[1] = 0x70;  // NVB: full UID cascade (4 bytes + BCC)
        sel[2] = lvlUID[0];
        sel[3] = lvlUID[1];
        sel[4] = lvlUID[2];
        sel[5] = lvlUID[3];
        sel[6] = lvlUID[0] ^ lvlUID[1] ^ lvlUID[2] ^ lvlUID[3]; // BCC

        uint8_t crc[2];
        result = calcCRC(sel, 7, crc);
        if (result != NIUS_OK) { return result; }
        sel[7] = crc[0];
        sel[8] = crc[1];

        uint8_t selBack[3];
        uint8_t selBackLen = sizeof(selBack);
        uint8_t selBits    = 0;

        result = executeCommand(MFRC522_CMD_TRANSCEIVE, 0x30,
                                sel, 9, selBack, &selBackLen,
                                &selBits, 0, true);
        if (result != NIUS_OK) { return result; }
        if (selBackLen != 3)   { return NIUS_ERR_UNKNOWN; }

        *outSAK = selBack[0];

        /* SAK bit 2 set → cascade tag in UID → continue to next level */
        uidComplete = ((*outSAK & 0x04) == 0);
    }

    *outLen = uidIdx;
    return NIUS_OK;
}

/* =======================================================================
 * Private — Card-type detection from SAK
 * ====================================================================== */

uint8_t NiusRC522::sakToCardType(uint8_t sak) {
    if      (sak & 0x20)          { return NIUS_CARD_ISO14443_4;  }
    else if (sak & 0x40)          { return NIUS_CARD_ISO18092;    }
    else if ((sak & 0x7F) == 0x00){ return NIUS_CARD_MIFARE_UL;  }
    else if ((sak & 0x7F) == 0x09){ return NIUS_CARD_MIFARE_MINI;}
    else if ((sak & 0x7F) == 0x08){ return NIUS_CARD_MIFARE_1K;  }
    else if ((sak & 0x7F) == 0x18){ return NIUS_CARD_MIFARE_4K;  }
    else if ((sak & 0x7F) == 0x10 ||
             (sak & 0x7F) == 0x11){ return NIUS_CARD_MIFARE_PLUS;}
    else                           { return NIUS_CARD_UNKNOWN;    }
}
