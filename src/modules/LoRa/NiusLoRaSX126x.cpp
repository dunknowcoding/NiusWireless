/*
 * NiusLoRaSX126x.cpp — SX1261 / SX1262 / SX1268 full driver implementation
 */

#include "NiusLoRaSX126x.h"

/* =======================================================================
 * Constructor
 * ====================================================================== */

NiusLoRaSX126x::NiusLoRaSX126x(uint8_t csPin, uint8_t rstPin,
                                uint8_t busyPin, uint8_t dio1Pin)
    : _csPin(csPin), _rstPin(rstPin), _busyPin(busyPin), _dio1Pin(dio1Pin),
      _spiSpeed(SX126X_DEFAULT_SPI_SPEED),
      _ready(false), _freqMHz(915.0f),
      _txBufLen(0), _rxBufLen(0), _rxBufPtr(0),
      _implicitHeader(false), _payloadLen(255),
      _bwCode(4),   // 125 kHz
      _sfCode(7), _crCode(1), _crcOn(true),
      _preambleLen(NIUS_LORA_DEFAULT_PREAMBLE)
{
    lastError = NIUS_OK;
    lastRxLen = 0;
    lastRssi  = 0;
    lastSnr   = 0.0f;
}

/* =======================================================================
 * NiusBase interface
 * ====================================================================== */

bool NiusLoRaSX126x::begin() {
    return begin(915.0f, SX126X_DEFAULT_SPI_SPEED);
}

bool NiusLoRaSX126x::begin(float freqMHz) {
    return begin(freqMHz, SX126X_DEFAULT_SPI_SPEED);
}

bool NiusLoRaSX126x::begin(float freqMHz, uint32_t spiSpeed) {
    _spiSpeed = spiSpeed;
    _freqMHz  = freqMHz;

    /* ---- GPIO setup -------------------------------------------------- */
    pinMode(_csPin,   OUTPUT); csHigh();
    pinMode(_rstPin,  OUTPUT);
    pinMode(_busyPin, INPUT);
    if (_dio1Pin != 0xFF) { pinMode(_dio1Pin, INPUT); }

    SPI.begin();

    /* ---- Hardware reset ---------------------------------------------- */
    reset();

    /* ---- Standby (RC oscillator) ------------------------------------- */
    {
        uint8_t p = SX126X_STANDBY_RC;
        sendCommand(SX126X_CMD_SET_STANDBY, &p, 1);
    }
    waitBusy();

    /* ---- Set LoRa packet type ---------------------------------------- */
    {
        uint8_t p = SX126X_PACKET_TYPE_LORA;
        sendCommand(SX126X_CMD_SET_PACKET_TYPE, &p, 1);
    }

    /* ---- LDO regulator (safe default; override with setRegulatorMode) */
    setRegulatorMode(false);

    /* ---- Calibrate all blocks ---------------------------------------- */
    {
        uint8_t p = 0x7F;  // all blocks
        sendCommand(SX126X_CMD_CALIBRATE, &p, 1);
    }
    waitBusy();

    /* ---- Calibrate image for the selected band ----------------------- */
    {
        uint8_t p[2];
        if      (freqMHz >= 902.0f) { p[0] = 0xE1; p[1] = 0xE9; }
        else if (freqMHz >= 863.0f) { p[0] = 0xD7; p[1] = 0xD8; }
        else if (freqMHz >= 779.0f) { p[0] = 0xC1; p[1] = 0xC5; }
        else if (freqMHz >= 470.0f) { p[0] = 0x75; p[1] = 0x81; }
        else                         { p[0] = 0x6B; p[1] = 0x6F; }
        sendCommand(SX126X_CMD_CALIBRATE_IMAGE, p, 2);
    }
    waitBusy();

    /* ---- PA config: SX1262 max 22 dBm -------------------------------- */
    {
        uint8_t p[4] = {
            SX1262_PA_DUTY_CYCLE_22DBM,
            SX1262_PA_HP_MAX_22DBM,
            SX1262_DEVICE_SEL,
            SX126X_PA_LUT_DEFAULT
        };
        sendCommand(SX126X_CMD_SET_PA_CONFIG, p, 4);
    }

    /* ---- Default modulation / packet params -------------------------- */
    setFrequency(freqMHz);
    setTxPower(NIUS_LORA_DEFAULT_POWER);
    applyModulationParams();
    applyPacketParams();
    setPreambleLength(NIUS_LORA_DEFAULT_PREAMBLE);
    setSyncWord(SX127X_SYNC_PRIVATE);

    /* ---- Set buffer base addresses ----------------------------------- */
    {
        uint8_t p[2] = {0x00, 0x00};
        sendCommand(SX126X_CMD_SET_BUFFER_BASE_ADDR, p, 2);
    }

    /* ---- DIO1 IRQ mask: TxDone | RxDone | Timeout | CrcErr ---------- */
    {
        uint16_t irqMask = SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_DONE |
                           SX126X_IRQ_TIMEOUT  | SX126X_IRQ_CRC_ERROR;
        uint8_t p[8] = {
            (uint8_t)(irqMask >> 8), (uint8_t)irqMask,  // IRQ mask
            (uint8_t)(irqMask >> 8), (uint8_t)irqMask,  // DIO1 mask
            0x00, 0x00,                                  // DIO2 mask (off)
            0x00, 0x00                                   // DIO3 mask (off)
        };
        sendCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, p, 8);
    }

    /* ---- Verify chip is alive via GetStatus -------------------------- */
    uint8_t status = 0;
    sendCommandReadResponse(SX126X_CMD_GET_STATUS, nullptr, 0, &status, 1);
    if (status == 0x00 || status == 0xFF) { return false; }

    _ready = true;
    return true;
}

bool NiusLoRaSX126x::isReady() {
    if (!_ready) { return false; }
    uint8_t status = 0;
    sendCommandReadResponse(SX126X_CMD_GET_STATUS, nullptr, 0, &status, 1);
    return (status != 0x00 && status != 0xFF);
}

void NiusLoRaSX126x::reset() {
    digitalWrite(_rstPin, LOW);
    delay(20);
    digitalWrite(_rstPin, HIGH);
    delay(10);
    waitBusy();
}

String NiusLoRaSX126x::getVersion() {
    return "SX126x";
}

/* =======================================================================
 * Frequency and modulation
 * ====================================================================== */

uint8_t NiusLoRaSX126x::setFrequency(float freqMHz) {
    _freqMHz = freqMHz;
    // frf = freq * 2^25 / 32e6 = freq * 1048.576
    uint32_t frf = (uint32_t)(freqMHz * 1048576.0f + 0.5f);
    uint8_t p[4] = {
        (uint8_t)(frf >> 24),
        (uint8_t)(frf >> 16),
        (uint8_t)(frf >>  8),
        (uint8_t) frf
    };
    sendCommand(SX126X_CMD_SET_RF_FREQUENCY, p, 4);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setBandwidth(uint32_t bwHz) {
    _bwCode = bwToCode(bwHz);
    applyModulationParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setSpreadingFactor(uint8_t sf) {
    if (sf < 5 || sf > 12) { return NIUS_LORA_ERR_PARAM; }
    _sfCode = sf;
    applyModulationParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setCodingRate(uint8_t cr) {
    if (cr < 5 || cr > 8) { return NIUS_LORA_ERR_PARAM; }
    _crCode = cr - 4;   // map 5→1, 6→2, 7→3, 8→4
    applyModulationParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setTxPower(int8_t dBm) {
    if (dBm > 22) { dBm = 22; }
    if (dBm < -9) { dBm = -9; }
    // SetTxParams: power (signed byte), rampTime
    uint8_t p[2] = { (uint8_t)dBm, 0x04 };  // ramp 200 µs
    sendCommand(SX126X_CMD_SET_TX_PARAMS, p, 2);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setPreambleLength(uint16_t length) {
    _preambleLen = length;
    applyPacketParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setSyncWord(uint8_t syncWord) {
    // LoRaWAN public = 0x34 → stored as 0x3444
    // Private = 0x12 → stored as 0x1424
    uint16_t sw = ((uint16_t)syncWord << 8) | ((syncWord & 0xF0) | 0x04);
    uint8_t bytes[2] = { (uint8_t)(sw >> 8), (uint8_t)sw };
    writeReg(SX126X_REG_LORA_SYNC_WORD_MSB, bytes, 2);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::enableCRC() {
    _crcOn = true;
    applyPacketParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::disableCRC() {
    _crcOn = false;
    applyPacketParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setExplicitHeader() {
    _implicitHeader = false;
    applyPacketParams();
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setImplicitHeader(uint8_t payloadLen) {
    _implicitHeader = true;
    _payloadLen = payloadLen;
    applyPacketParams();
    return NIUS_LORA_OK;
}

/* =======================================================================
 * Packet TX
 * ====================================================================== */

uint8_t NiusLoRaSX126x::beginPacket() {
    standby();
    _txBufLen = 0;
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::write(uint8_t byte) {
    if (_txBufLen >= SX126X_MAX_PAYLOAD) { return 0; }
    _txBuf[_txBufLen++] = byte;
    return 1;
}

uint8_t NiusLoRaSX126x::writeBuf(uint8_t *data, uint8_t len) {
    uint8_t n = 0;
    while (n < len && _txBufLen < SX126X_MAX_PAYLOAD) {
        _txBuf[_txBufLen++] = data[n++];
    }
    return n;
}

uint8_t NiusLoRaSX126x::endPacket(bool async) {
    // Write payload to TX buffer at address 0x00
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(SX126X_CMD_WRITE_BUFFER);
    spiTransfer(0x00);  // offset in FIFO
    for (uint8_t i = 0; i < _txBufLen; i++) { spiTransfer(_txBuf[i]); }
    SPI.endTransaction();
    csHigh();
    waitBusy();

    // Update payload length for explicit header
    if (!_implicitHeader) {
        _payloadLen = _txBufLen;
        applyPacketParams();
    }

    // Clear IRQ, start TX (no timeout = 0x000000)
    {
        uint8_t clr[2] = { 0xFF, 0xFF };
        sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    }
    {
        uint8_t p[3] = { 0x00, 0x00, 0x00 };  // no timeout
        sendCommand(SX126X_CMD_SET_TX, p, 3);
    }

    if (!async) {
        uint32_t deadline = millis() + 2000;
        while (millis() < deadline) {
            uint8_t irqRaw[3];
            sendCommandReadResponse(SX126X_CMD_GET_IRQ_STATUS, nullptr, 0,
                                    irqRaw, 3);
            uint16_t irq = ((uint16_t)irqRaw[1] << 8) | irqRaw[2];
            if (irq & SX126X_IRQ_TX_DONE) {
                uint8_t clr[2] = { 0xFF, 0xFF };
                sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
                break;
            }
        }
        standby();
    }
    return NIUS_LORA_OK;
}

bool NiusLoRaSX126x::isTxDone() {
    uint8_t irqRaw[3];
    sendCommandReadResponse(SX126X_CMD_GET_IRQ_STATUS, nullptr, 0, irqRaw, 3);
    uint16_t irq = ((uint16_t)irqRaw[1] << 8) | irqRaw[2];
    if (irq & SX126X_IRQ_TX_DONE) {
        uint8_t clr[2] = { 0xFF, 0xFF };
        sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
        standby();
        return true;
    }
    return false;
}

/* =======================================================================
 * Packet RX
 * ====================================================================== */

uint8_t NiusLoRaSX126x::startReceive() {
    uint8_t clr[2] = { 0xFF, 0xFF };
    sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    uint8_t p[3] = { 0xFF, 0xFF, 0xFF };  // continuous RX (no timeout)
    sendCommand(SX126X_CMD_SET_RX, p, 3);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::startReceiveSingle() {
    uint8_t clr[2] = { 0xFF, 0xFF };
    sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    // Timeout = 25 * 15.625 µs * period — use ~500 ms (0x8000 * 15.625 µs ≈ 512 ms)
    uint8_t p[3] = { 0x00, 0x80, 0x00 };
    sendCommand(SX126X_CMD_SET_RX, p, 3);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::parsePacket() {
    uint8_t irqRaw[3];
    sendCommandReadResponse(SX126X_CMD_GET_IRQ_STATUS, nullptr, 0, irqRaw, 3);
    uint16_t irq = ((uint16_t)irqRaw[1] << 8) | irqRaw[2];

    if (irq & SX126X_IRQ_RX_DONE) {
        uint8_t clr[2] = { 0xFF, 0xFF };
        sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);

        if (irq & SX126X_IRQ_CRC_ERROR) {
            lastError = NIUS_LORA_ERR_CRC;
            return 0;
        }

        // GetRxBufferStatus: payloadLen, startAddr
        uint8_t rxStatus[3];
        sendCommandReadResponse(SX126X_CMD_GET_RX_BUFFER_STATUS,
                                nullptr, 0, rxStatus, 3);
        _rxBufLen = rxStatus[1];
        uint8_t startAddr = rxStatus[2];

        // Read payload from buffer
        if (_rxBufLen > SX126X_MAX_PAYLOAD) { _rxBufLen = SX126X_MAX_PAYLOAD; }
        csLow();
        SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
        spiTransfer(SX126X_CMD_READ_BUFFER);
        spiTransfer(startAddr);
        spiTransfer(0x00);  // NOP
        for (uint8_t i = 0; i < _rxBufLen; i++) {
            _rxBuf[i] = spiTransfer(0x00);
        }
        SPI.endTransaction();
        csHigh();
        waitBusy();

        _rxBufPtr = 0;

        // GetPacketStatus
        uint8_t pktStatus[4];
        sendCommandReadResponse(SX126X_CMD_GET_PACKET_STATUS,
                                nullptr, 0, pktStatus, 4);
        // pktStatus[1] = RSSI_pkt / -2
        // pktStatus[2] = SNR_pkt / 4 (signed)
        lastRssi = -(int16_t)(pktStatus[1]) / 2;
        lastSnr  = (int8_t)pktStatus[2] / 4.0f;
        lastRxLen = _rxBufLen;
        lastError = NIUS_LORA_OK;
        return _rxBufLen;
    }
    return 0;
}

uint8_t NiusLoRaSX126x::available() {
    return (uint8_t)(_rxBufLen - _rxBufPtr);
}

int NiusLoRaSX126x::readByte() {
    if (_rxBufPtr >= _rxBufLen) { return -1; }
    return (int)_rxBuf[_rxBufPtr++];
}

uint8_t NiusLoRaSX126x::readBuf(uint8_t *buf, uint8_t len) {
    uint8_t n = 0;
    while (n < len && _rxBufPtr < _rxBufLen) {
        buf[n++] = _rxBuf[_rxBufPtr++];
    }
    return n;
}

/* =======================================================================
 * Signal quality
 * ====================================================================== */

int16_t NiusLoRaSX126x::getRSSI()     { return lastRssi; }
float   NiusLoRaSX126x::getSNR()      { return lastSnr;  }

int16_t NiusLoRaSX126x::getLastRSSI() {
    uint8_t rssiRaw[2];
    sendCommandReadResponse(SX126X_CMD_GET_RSSI_INST, nullptr, 0, rssiRaw, 2);
    return -(int16_t)(rssiRaw[1]) / 2;
}

/* =======================================================================
 * Power management
 * ====================================================================== */

uint8_t NiusLoRaSX126x::sleep() {
    uint8_t p = 0x00;  // cold start after wakeup
    sendCommand(SX126X_CMD_SET_SLEEP, &p, 1);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::standby() {
    uint8_t p = SX126X_STANDBY_RC;
    sendCommand(SX126X_CMD_SET_STANDBY, &p, 1);
    waitBusy();
    return NIUS_LORA_OK;
}

/* =======================================================================
 * CAD
 * ====================================================================== */

bool NiusLoRaSX126x::isChannelActive() {
    uint8_t clr[2] = { 0xFF, 0xFF };
    sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    sendCommand(SX126X_CMD_SET_CAD, nullptr, 0);

    uint32_t deadline = millis() + 500;
    while (millis() < deadline) {
        uint8_t irqRaw[3];
        sendCommandReadResponse(SX126X_CMD_GET_IRQ_STATUS, nullptr, 0,
                                irqRaw, 3);
        uint16_t irq = ((uint16_t)irqRaw[1] << 8) | irqRaw[2];
        if (irq & SX126X_IRQ_CAD_DONE) {
            sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
            return (irq & SX126X_IRQ_CAD_DETECTED) != 0;
        }
    }
    standby();
    return false;
}

/* =======================================================================
 * Advanced
 * ====================================================================== */

uint8_t NiusLoRaSX126x::setRegulatorMode(bool useDCDC) {
    uint8_t p = useDCDC ? SX126X_REGULATOR_DCDC : SX126X_REGULATOR_LDO;
    sendCommand(SX126X_CMD_SET_REGULATOR_MODE, &p, 1);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setDIO2AsRFSwitch(bool enable) {
    uint8_t p = enable ? 0x01 : 0x00;
    sendCommand(SX126X_CMD_SET_DIO2_AS_RF_SWITCH, &p, 1);
    return NIUS_LORA_OK;
}

uint8_t NiusLoRaSX126x::setTCXO(float voltage, uint32_t delayMs) {
    if (voltage == 0.0f) {
        // Disable TCXO: standby with RC
        standby();
        return NIUS_LORA_OK;
    }
    // Map voltage to TCXO code
    uint8_t vCode;
    if      (voltage < 1.7f) { vCode = 0x00; }
    else if (voltage < 1.9f) { vCode = 0x01; }
    else if (voltage < 2.1f) { vCode = 0x02; }
    else if (voltage < 2.3f) { vCode = 0x03; }
    else if (voltage < 2.5f) { vCode = 0x04; }
    else if (voltage < 2.7f) { vCode = 0x05; }
    else if (voltage < 3.0f) { vCode = 0x06; }
    else                      { vCode = 0x07; }
    // Delay in 15.625 µs steps
    uint32_t delay = (delayMs * 1000) / 15625UL;
    uint8_t p[4] = {
        vCode,
        (uint8_t)(delay >> 16),
        (uint8_t)(delay >> 8),
        (uint8_t) delay
    };
    sendCommand(SX126X_CMD_SET_DIO3_AS_TCXO_CTRL, p, 4);
    return NIUS_LORA_OK;
}

void NiusLoRaSX126x::handleDIO1() {
    uint8_t irqRaw[3];
    sendCommandReadResponse(SX126X_CMD_GET_IRQ_STATUS, nullptr, 0, irqRaw, 3);
    uint16_t irq = ((uint16_t)irqRaw[1] << 8) | irqRaw[2];
    uint8_t clr[2] = { 0xFF, 0xFF };
    sendCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    if ((irq & SX126X_IRQ_TX_DONE) && _onTxDone) { _onTxDone(); }
    if ((irq & SX126X_IRQ_RX_DONE) && _onRxDone) { _onRxDone(); }
}

/* =======================================================================
 * Raw SPI
 * ====================================================================== */

void NiusLoRaSX126x::sendCommand(uint8_t cmd, uint8_t *params, uint8_t nParams) {
    waitBusy();
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(cmd);
    for (uint8_t i = 0; i < nParams; i++) { spiTransfer(params[i]); }
    SPI.endTransaction();
    csHigh();
}

void NiusLoRaSX126x::sendCommandReadResponse(uint8_t cmd,
                                              uint8_t *params, uint8_t nParams,
                                              uint8_t *response, uint8_t nResponse) {
    waitBusy();
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(cmd);
    for (uint8_t i = 0; i < nParams; i++)    { spiTransfer(params[i]); }
    spiTransfer(0x00);  // NOP (status byte returned here, discarded)
    for (uint8_t i = 0; i < nResponse; i++)  { response[i] = spiTransfer(0x00); }
    SPI.endTransaction();
    csHigh();
}

void NiusLoRaSX126x::writeReg(uint16_t addr, uint8_t *data, uint8_t len) {
    waitBusy();
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(SX126X_CMD_WRITE_REGISTER);
    spiTransfer((uint8_t)(addr >> 8));
    spiTransfer((uint8_t) addr);
    for (uint8_t i = 0; i < len; i++) { spiTransfer(data[i]); }
    SPI.endTransaction();
    csHigh();
}

void NiusLoRaSX126x::readReg(uint16_t addr, uint8_t *data, uint8_t len) {
    waitBusy();
    csLow();
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    spiTransfer(SX126X_CMD_READ_REGISTER);
    spiTransfer((uint8_t)(addr >> 8));
    spiTransfer((uint8_t) addr);
    spiTransfer(0x00);  // NOP
    for (uint8_t i = 0; i < len; i++) { data[i] = spiTransfer(0x00); }
    SPI.endTransaction();
    csHigh();
}

/* =======================================================================
 * Private helpers
 * ====================================================================== */

void NiusLoRaSX126x::waitBusy() {
    uint32_t deadline = millis() + 1000;
    while (digitalRead(_busyPin) == HIGH && millis() < deadline) {
        delayMicroseconds(10);
    }
}

void NiusLoRaSX126x::csLow()  { digitalWrite(_csPin, LOW);  }
void NiusLoRaSX126x::csHigh() { digitalWrite(_csPin, HIGH); }

uint8_t NiusLoRaSX126x::spiTransfer(uint8_t data) {
    return SPI.transfer(data);
}

void NiusLoRaSX126x::applyModulationParams() {
    // LDRO: auto-enable when symbol time > 16 ms
    // sym_time_ms = 2^SF / BW_Hz * 1000
    // BW in kHz from code
    static const uint32_t bwTable[] = {
        500000UL, 250000UL, 125000UL, 62500UL, 41670UL,
        31250UL,  20830UL,  15630UL,  10420UL, 7810UL
    };
    uint32_t bwHz = (_bwCode < 10) ? bwTable[_bwCode] : 125000UL;
    float symTime = (1 << _sfCode) / (float)bwHz * 1000.0f;
    uint8_t ldro = (symTime >= 16.0f) ? 0x01 : 0x00;

    uint8_t p[4] = { _sfCode, _bwCode, _crCode, ldro };
    sendCommand(SX126X_CMD_SET_MODULATION_PARAMS, p, 4);
}

void NiusLoRaSX126x::applyPacketParams() {
    // SetPacketParams: preamble[15:8], preamble[7:0], headerType,
    //                  payloadLen, crcType, invertIQ
    uint8_t p[6] = {
        (uint8_t)(_preambleLen >> 8),
        (uint8_t) _preambleLen,
        _implicitHeader ? 0x01 : 0x00,
        _payloadLen,
        _crcOn ? 0x01 : 0x00,
        0x00   // normal IQ
    };
    sendCommand(SX126X_CMD_SET_PACKET_PARAMS, p, 6);
}

uint8_t NiusLoRaSX126x::bwToCode(uint32_t bwHz) {
    // SX126x BW codes
    if (bwHz <=   7810) { return 9; }
    if (bwHz <=  10420) { return 8; }
    if (bwHz <=  15630) { return 7; }
    if (bwHz <=  20830) { return 6; }
    if (bwHz <=  31250) { return 5; }
    if (bwHz <=  41670) { return 4; }
    if (bwHz <=  62500) { return 3; }
    if (bwHz <= 125000) { return 2; }
    if (bwHz <= 250000) { return 1; }
    return 0;  // 500 kHz
}
