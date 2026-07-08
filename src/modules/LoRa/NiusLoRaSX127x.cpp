/*
 * NiusLoRaSX127x.cpp — SX1276 / SX1277 / SX1278 full driver implementation
 */

#include "NiusLoRaSX127x.h"

/* =======================================================================
 * Constructor
 * ====================================================================== */

NiusLoRaSX127x::NiusLoRaSX127x(uint8_t csPin, uint8_t rstPin, uint8_t dio0Pin)
    : _csPin(csPin), _rstPin(rstPin), _dio0Pin(dio0Pin),
      _spiSpeed(SX127X_DEFAULT_SPI_SPEED),
      _ready(false), _freqMHz(915.0f),
      _txBufLen(0), _rxBufPtr(0), _rxBufLen(0),
      _implicitHeader(false), _txAsync(false)
{
    lastError = NIUS_OK;
    lastRxLen = 0;
    lastRssi  = 0;
    lastSnr   = 0.0f;
}

/* =======================================================================
 * NiusBase interface
 * ====================================================================== */

bool NiusLoRaSX127x::begin() {
    return begin(915.0f, SX127X_DEFAULT_SPI_SPEED);
}

bool NiusLoRaSX127x::begin(float freqMHz) {
    return begin(freqMHz, SX127X_DEFAULT_SPI_SPEED);
}

bool NiusLoRaSX127x::begin(float freqMHz, uint32_t spiSpeed) {
    _spiSpeed = spiSpeed;
    _freqMHz  = freqMHz;

    /* ---- GPIO setup -------------------------------------------------- */
    pinMode(_csPin, OUTPUT);
    csHigh();
    pinMode(_rstPin, OUTPUT);
    if (_dio0Pin != 0xFF) { pinMode(_dio0Pin, INPUT); }

    SPI.begin();

    /* ---- Hardware reset ---------------------------------------------- */
    reset();

    /* ---- Verify chip version ----------------------------------------- */
    uint8_t ver = readRegister(SX127X_REG_VERSION);
    if (ver == 0x00 || ver == 0xFF) { return false; }

    /* ---- Enter sleep, then set LoRa mode ----------------------------- */
    writeRegister(SX127X_REG_OP_MODE, SX127X_OPMODE_SLEEP);
    delay(10);
    // Set LoRa mode bit while in sleep (required)
    writeRegister(SX127X_REG_OP_MODE, SX127X_OPMODE_SLEEP | SX127X_OPMODE_LORA);
    delay(10);

    /* ---- Standby ----------------------------------------------------- */
    writeRegister(SX127X_REG_OP_MODE, SX127X_OPMODE_LORA | SX127X_OPMODE_STDBY);

    /* ---- Set FIFO base addresses ------------------------------------- */
    writeRegister(SX127X_REG_FIFO_TX_BASE, 0x00);
    writeRegister(SX127X_REG_FIFO_RX_BASE, 0x00);

    /* ---- LNA: max gain, boost at HF ---------------------------------- */
    writeRegister(SX127X_REG_LNA, 0x23);  // G1 (max), boost HF on

    /* ---- Default modulation params ----------------------------------- */
    setFrequency(freqMHz);
    setTxPower(NIUS_LORA_DEFAULT_POWER);
    setBandwidth(NIUS_LORA_DEFAULT_BW);
    setSpreadingFactor(NIUS_LORA_DEFAULT_SF);
    setCodingRate(NIUS_LORA_DEFAULT_CR);
    setPreambleLength(NIUS_LORA_DEFAULT_PREAMBLE);
    enableCRC();
    setExplicitHeader();
    setSyncWord(SX127X_SYNC_PRIVATE);

    /* ---- ModemConfig3: LDRO auto, AGC on ----------------------------- */
    writeRegister(SX127X_REG_MODEM_CONFIG3, 0x04);  // AGC on

    /* ---- Max payload ------------------------------------------------- */
    writeRegister(SX127X_REG_MAX_PAYLOAD, 0xFF);

    /* ---- DIO0 mapping: TxDone in TX, RxDone in RX ------------------- */
    writeRegister(SX127X_REG_DIO_MAPPING1, 0x00);

    _ready = true;
    return true;
}

bool NiusLoRaSX127x::isReady() {
    if (!_ready) { return false; }
    uint8_t v = readRegister(SX127X_REG_VERSION);
    return (v != 0x00 && v != 0xFF);
}

void NiusLoRaSX127x::reset() {
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(10);
}

String NiusLoRaSX127x::getVersion() {
    uint8_t ver = readRegister(SX127X_REG_VERSION);
    String s = "SX127x v0x";
    if (ver < 0x10) { s += '0'; }
    s += String(ver, HEX);
    return s;
}

/* =======================================================================
 * Frequency and modulation
 * ====================================================================== */

uint8_t NiusLoRaSX127x::setFrequency(float freqMHz) {
    _freqMHz = freqMHz;
    // FRF = freq * 2^19 / 32e6 = freq * 16384.0
    uint32_t frf = (uint32_t)(freqMHz * 16384.0f + 0.5f) << 3;
    writeRegister(SX127X_REG_FR_MSB, (frf >> 16) & 0xFF);
    writeRegister(SX127X_REG_FR_MID, (frf >>  8) & 0xFF);
    writeRegister(SX127X_REG_FR_LSB,  frf        & 0xFF);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setBandwidth(uint32_t bwHz) {
    bool lf;
    uint8_t bwBits = bwToBits(bwHz, lf);
    uint8_t mc1 = readRegister(SX127X_REG_MODEM_CONFIG1);
    mc1 = (mc1 & 0x0F) | (bwBits << 4);
    writeRegister(SX127X_REG_MODEM_CONFIG1, mc1);
    if (lf) {
        setRegisterBits(SX127X_REG_OP_MODE, SX127X_OPMODE_LF);
    } else {
        clearRegisterBits(SX127X_REG_OP_MODE, SX127X_OPMODE_LF);
    }
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setSpreadingFactor(uint8_t sf) {
    if (sf < 6 || sf > 12) { return NIUS_LORA_ERR_PARAM; }
    uint8_t mc2 = readRegister(SX127X_REG_MODEM_CONFIG2);
    mc2 = (mc2 & 0x0F) | ((sf << 4) & 0xF0);
    writeRegister(SX127X_REG_MODEM_CONFIG2, mc2);
    // SF6 requires detection optimisation
    if (sf == 6) {
        writeRegister(0x31, 0xC5);
        writeRegister(0x37, 0x0C);
    } else {
        writeRegister(0x31, 0xC3);
        writeRegister(0x37, 0x0A);
    }
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setCodingRate(uint8_t cr) {
    if (cr < 5 || cr > 8) { return NIUS_LORA_ERR_PARAM; }
    uint8_t bits = (cr - 4) << 1;   // map 5→2, 6→4, 7→6, 8→8 (bits [3:1])
    uint8_t mc1 = readRegister(SX127X_REG_MODEM_CONFIG1);
    mc1 = (mc1 & 0xF1) | bits;
    writeRegister(SX127X_REG_MODEM_CONFIG1, mc1);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setTxPower(int8_t dBm) {
    if (dBm > 17) {
        // +20 dBm mode via PA_DAC
        if (dBm > 20) { dBm = 20; }
        writeRegister(SX127X_REG_PA_DAC, SX127X_PA_DAC_20DBM);
        setOCP(140);
        uint8_t outputPower = (uint8_t)(dBm - 5);  // Maps 18→13, 19→14, 20→15
        writeRegister(SX127X_REG_PA_CONFIG,
                      SX127X_PA_SELECT_BOOST | 0x70 | (outputPower & 0x0F));
    } else if (dBm > 14) {
        // PA_BOOST: 2..17 dBm → OutputPower 0..15
        writeRegister(SX127X_REG_PA_DAC, SX127X_PA_DAC_DEFAULT);
        setOCP(100);
        if (dBm < 2) { dBm = 2; }
        writeRegister(SX127X_REG_PA_CONFIG,
                      SX127X_PA_SELECT_BOOST | 0x70 | ((dBm - 2) & 0x0F));
    } else {
        // RFO pin: -1..14 dBm
        writeRegister(SX127X_REG_PA_DAC, SX127X_PA_DAC_DEFAULT);
        setOCP(100);
        if (dBm < -1) { dBm = -1; }
        if (dBm > 14) { dBm = 14; }
        writeRegister(SX127X_REG_PA_CONFIG,
                      SX127X_PA_SELECT_RFO | 0x70 | ((dBm + 1) & 0x0F));
    }
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setPreambleLength(uint16_t length) {
    writeRegister(SX127X_REG_PREAMBLE_MSB, (length >> 8) & 0xFF);
    writeRegister(SX127X_REG_PREAMBLE_LSB,  length       & 0xFF);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setSyncWord(uint8_t syncWord) {
    writeRegister(SX127X_REG_SYNC_WORD, syncWord);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::enableCRC() {
    setRegisterBits(SX127X_REG_MODEM_CONFIG2, 0x04);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::disableCRC() {
    clearRegisterBits(SX127X_REG_MODEM_CONFIG2, 0x04);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setExplicitHeader() {
    _implicitHeader = false;
    clearRegisterBits(SX127X_REG_MODEM_CONFIG1, 0x01);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setImplicitHeader(uint8_t payloadLen) {
    _implicitHeader = true;
    setRegisterBits(SX127X_REG_MODEM_CONFIG1, 0x01);
    writeRegister(SX127X_REG_PAYLOAD_LENGTH, payloadLen);
    return NIUS_LORA_OK;
}

/* =======================================================================
 * Packet TX
 * ====================================================================== */

uint8_t NiusLoRaSX127x::beginPacket() {
    standby();
    // Reset FIFO pointer to TX base
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, 0x00);
    writeRegister(SX127X_REG_PAYLOAD_LENGTH, 0x00);
    _txBufLen = 0;
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::write(uint8_t byte) {
    if (_txBufLen >= SX127X_MAX_PAYLOAD) { return 0; }
    writeRegister(SX127X_REG_FIFO, byte);
    _txBufLen++;
    return 1;
}

uint8_t NiusLoRaSX127x::writeBuf(uint8_t *data, uint8_t len) {
    uint8_t written = 0;
    for (uint8_t i = 0; i < len && _txBufLen < SX127X_MAX_PAYLOAD; i++) {
        writeRegister(SX127X_REG_FIFO, data[i]);
        _txBufLen++;
        written++;
    }
    return written;
}

uint8_t NiusLoRaSX127x::endPacket(bool async) {
    _txAsync = async;
    writeRegister(SX127X_REG_PAYLOAD_LENGTH, _txBufLen);
    // DIO0 = TxDone
    writeRegister(SX127X_REG_DIO_MAPPING1, 0x40);
    // Clear IRQ flags
    writeRegister(SX127X_REG_IRQ_FLAGS, 0xFF);
    // Start TX
    setOpMode(SX127X_OPMODE_TX);

    if (!async) {
        // Block until TxDone IRQ
        uint32_t deadline = millis() + 2000;
        while (millis() < deadline) {
            if (readRegister(SX127X_REG_IRQ_FLAGS) & SX127X_IRQ_TX_DONE) { break; }
        }
        writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_TX_DONE);
        standby();
    }
    return NIUS_LORA_OK;
}

bool NiusLoRaSX127x::isTxDone() {
    if (readRegister(SX127X_REG_IRQ_FLAGS) & SX127X_IRQ_TX_DONE) {
        writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_TX_DONE);
        standby();
        return true;
    }
    return false;
}

/* =======================================================================
 * Packet RX
 * ====================================================================== */

uint8_t NiusLoRaSX127x::startReceive() {
    // DIO0 = RxDone
    writeRegister(SX127X_REG_DIO_MAPPING1, 0x00);
    writeRegister(SX127X_REG_IRQ_FLAGS, 0xFF);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, 0x00);
    setOpMode(SX127X_OPMODE_RX_CONT);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::startReceiveSingle() {
    writeRegister(SX127X_REG_DIO_MAPPING1, 0x00);
    writeRegister(SX127X_REG_IRQ_FLAGS, 0xFF);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, 0x00);
    setOpMode(SX127X_OPMODE_RX_SINGLE);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::parsePacket() {
    uint8_t irq = readRegister(SX127X_REG_IRQ_FLAGS);

    if (irq & SX127X_IRQ_RX_DONE) {
        writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_RX_DONE);

        // Check for CRC error
        if (irq & SX127X_IRQ_PAYLOAD_CRC_ERR) {
            writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_PAYLOAD_CRC_ERR);
            lastError = NIUS_LORA_ERR_CRC;
            return 0;
        }

        _rxBufLen = _implicitHeader
            ? readRegister(SX127X_REG_PAYLOAD_LENGTH)
            : readRegister(SX127X_REG_RX_NB_BYTES);

        // Seek to start of received packet
        writeRegister(SX127X_REG_FIFO_ADDR_PTR,
                      readRegister(SX127X_REG_FIFO_RX_CURRENT));
        _rxBufPtr = 0;

        // Cache signal quality
        int8_t snrRaw = (int8_t)readRegister(SX127X_REG_PKT_SNR_VALUE);
        lastSnr  = snrRaw / 4.0f;
        int16_t pktRssi = (int16_t)readRegister(SX127X_REG_PKT_RSSI_VALUE);
        if (lastSnr < 0) {
            lastRssi = -157 + pktRssi + (int16_t)lastSnr;
        } else {
            lastRssi = -157 + (int16_t)(pktRssi * 16 / 15);
        }
        lastRxLen = _rxBufLen;
        lastError = NIUS_LORA_OK;
        return _rxBufLen;
    }
    return 0;
}

uint8_t NiusLoRaSX127x::available() {
    return (uint8_t)(_rxBufLen - _rxBufPtr);
}

int NiusLoRaSX127x::readByte() {
    if (_rxBufPtr >= _rxBufLen) { return -1; }
    _rxBufPtr++;
    return (int)readRegister(SX127X_REG_FIFO);
}

uint8_t NiusLoRaSX127x::readBuf(uint8_t *buf, uint8_t len) {
    uint8_t n = 0;
    while (n < len && _rxBufPtr < _rxBufLen) {
        buf[n++] = (uint8_t)readRegister(SX127X_REG_FIFO);
        _rxBufPtr++;
    }
    return n;
}

/* =======================================================================
 * Signal quality
 * ====================================================================== */

int16_t NiusLoRaSX127x::getRSSI()     { return lastRssi; }
float   NiusLoRaSX127x::getSNR()      { return lastSnr;  }

int16_t NiusLoRaSX127x::getLastRSSI() {
    // Current ambient RSSI (not from a packet)
    return -157 + (int16_t)readRegister(SX127X_REG_RSSI_VALUE);
}

int32_t NiusLoRaSX127x::getFrequencyError() {
    // 24-bit signed value, convert to Hz
    int32_t raw = 0;
    raw  = (int32_t)(readRegister(0x28) & 0x0F) << 16;
    raw |= (int32_t) readRegister(0x29)          <<  8;
    raw |=           readRegister(0x2A);
    if (raw & 0x80000) { raw -= 0x100000; }  // sign-extend 20-bit
    // fError = raw * f_XOSC / 2^24 * BW / 500
    // Approximation for common BW values:
    return (int32_t)((float)raw * (float)_freqMHz * 1000.0f / (float)(1 << 19));
}

/* =======================================================================
 * Power management
 * ====================================================================== */

uint8_t NiusLoRaSX127x::sleep() {
    setOpMode(SX127X_OPMODE_SLEEP);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::standby() {
    setOpMode(SX127X_OPMODE_STDBY);
    return NIUS_LORA_OK;
}

/* =======================================================================
 * CAD
 * ====================================================================== */

bool NiusLoRaSX127x::isChannelActive() {
    // DIO0 = CadDone
    writeRegister(SX127X_REG_DIO_MAPPING1, 0x80);
    writeRegister(SX127X_REG_IRQ_FLAGS, 0xFF);
    setOpMode(SX127X_OPMODE_CAD);

    uint32_t deadline = millis() + 200;
    while (millis() < deadline) {
        uint8_t irq = readRegister(SX127X_REG_IRQ_FLAGS);
        if (irq & SX127X_IRQ_CAD_DONE) {
            writeRegister(SX127X_REG_IRQ_FLAGS,
                          SX127X_IRQ_CAD_DONE | SX127X_IRQ_CAD_DETECTED);
            return (irq & SX127X_IRQ_CAD_DETECTED) != 0;
        }
    }
    standby();
    return false;
}

/* =======================================================================
 * Advanced
 * ====================================================================== */

uint8_t NiusLoRaSX127x::setLNAGain(uint8_t gain) {
    uint8_t mc3 = readRegister(SX127X_REG_MODEM_CONFIG3);
    if (gain == 0) {
        // Re-enable AGC
        mc3 |= 0x04;
        writeRegister(SX127X_REG_MODEM_CONFIG3, mc3);
        writeRegister(SX127X_REG_LNA, 0x23);
    } else {
        if (gain > 6) { gain = 6; }
        mc3 &= ~0x04;  // Disable AGC
        writeRegister(SX127X_REG_MODEM_CONFIG3, mc3);
        writeRegister(SX127X_REG_LNA, (gain << 5) | 0x03);
    }
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX127x::setOCP(uint8_t mA) {
    uint8_t trim;
    if (mA <= 120) {
        trim = (mA - 45) / 5;
    } else {
        trim = (mA + 30) / 10;
    }
    if (trim > 27) { trim = 27; }
    writeRegister(SX127X_REG_OCP, 0x20 | (trim & 0x1F));
    return NIUS_LORA_OK;
}

/* =======================================================================
 * Raw register access
 * ====================================================================== */

uint8_t NiusLoRaSX127x::readRegister(uint8_t addr) {
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(addr & 0x7F);
    uint8_t val = spiTransfer(0x00);
    SPI.endTransaction();
    csHigh();
    return val;
}

void NiusLoRaSX127x::writeRegister(uint8_t addr, uint8_t value) {
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(addr | 0x80);
    spiTransfer(value);
    SPI.endTransaction();
    csHigh();
}

void NiusLoRaSX127x::setRegisterBits(uint8_t addr, uint8_t mask) {
    writeRegister(addr, readRegister(addr) | mask);
}

void NiusLoRaSX127x::clearRegisterBits(uint8_t addr, uint8_t mask) {
    writeRegister(addr, readRegister(addr) & (~mask));
}

/* =======================================================================
 * DIO0 ISR
 * ====================================================================== */

void NiusLoRaSX127x::handleDIO0() {
    uint8_t irq = readRegister(SX127X_REG_IRQ_FLAGS);
    if (irq & SX127X_IRQ_TX_DONE) {
        writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_TX_DONE);
        if (_onTxDone) { _onTxDone(); }
    }
    if (irq & SX127X_IRQ_RX_DONE) {
        writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_RX_DONE);
        if (_onRxDone) { _onRxDone(); }
    }
}

/* =======================================================================
 * Private helpers
 * ====================================================================== */

void NiusLoRaSX127x::csLow()  { digitalWrite(_csPin, LOW);  }
void NiusLoRaSX127x::csHigh() { digitalWrite(_csPin, HIGH); }

uint8_t NiusLoRaSX127x::spiTransfer(uint8_t data) {
    return SPI.transfer(data);
}

void NiusLoRaSX127x::setOpMode(uint8_t mode) {
    writeRegister(SX127X_REG_OP_MODE,
                  SX127X_OPMODE_LORA | mode |
                  ((_freqMHz < 600.0f) ? SX127X_OPMODE_LF : 0));
}

uint8_t NiusLoRaSX127x::bwToBits(uint32_t bwHz, bool &isLowFreq) {
    isLowFreq = (_freqMHz < 600.0f);
    if (bwHz <=   7800) { return 0; }
    if (bwHz <=  10400) { return 1; }
    if (bwHz <=  15600) { return 2; }
    if (bwHz <=  20800) { return 3; }
    if (bwHz <=  31250) { return 4; }
    if (bwHz <=  41700) { return 5; }
    if (bwHz <=  62500) { return 6; }
    if (bwHz <= 125000) { return 7; }
    if (bwHz <= 250000) { return 8; }
    return 9;  // 500 kHz (SX1276 HF only)
}
