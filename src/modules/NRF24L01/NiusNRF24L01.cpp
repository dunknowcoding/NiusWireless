/*
 * NiusNRF24L01.cpp — NRF24L01 / NRF24L01+ 2.4 GHz radio driver
 *
 * Register-level driver written against the Nordic nRF24L01+ product
 * specification. Enhanced ShockBurst (auto-acknowledge + auto-retransmit) is
 * enabled by default, and dynamic payload length is used whenever the silicon
 * accepts it so writeRadio()/readRadio() carry exactly the requested bytes.
 */

#include "NiusNRF24L01.h"

/* ----------------------------------------------------------------------- */
/* SPI commands                                                             */
/* ----------------------------------------------------------------------- */
#define NRF_CMD_R_REGISTER    0x00
#define NRF_CMD_W_REGISTER    0x20
#define NRF_CMD_R_RX_PAYLOAD  0x61
#define NRF_CMD_W_TX_PAYLOAD  0xA0
#define NRF_CMD_FLUSH_TX      0xE1
#define NRF_CMD_FLUSH_RX      0xE2
#define NRF_CMD_ACTIVATE      0x50
#define NRF_CMD_R_RX_PL_WID   0x60
#define NRF_CMD_NOP           0xFF
#define NRF_REGISTER_MASK     0x1F

/* ----------------------------------------------------------------------- */
/* Registers                                                                */
/* ----------------------------------------------------------------------- */
#define NRF_REG_CONFIG        0x00
#define NRF_REG_EN_AA         0x01
#define NRF_REG_EN_RXADDR     0x02
#define NRF_REG_SETUP_AW      0x03
#define NRF_REG_SETUP_RETR    0x04
#define NRF_REG_RF_CH         0x05
#define NRF_REG_RF_SETUP      0x06
#define NRF_REG_STATUS        0x07
#define NRF_REG_OBSERVE_TX    0x08
#define NRF_REG_RPD           0x09
#define NRF_REG_RX_ADDR_P0    0x0A
#define NRF_REG_RX_ADDR_P1    0x0B
#define NRF_REG_TX_ADDR       0x10
#define NRF_REG_RX_PW_P0      0x11
#define NRF_REG_FIFO_STATUS   0x17
#define NRF_REG_DYNPD         0x1C
#define NRF_REG_FEATURE       0x1D

/* CONFIG bits */
#define NRF_CONFIG_EN_CRC     0x08
#define NRF_CONFIG_CRCO       0x04
#define NRF_CONFIG_PWR_UP     0x02
#define NRF_CONFIG_PRIM_RX    0x01

/* STATUS bits */
#define NRF_STATUS_RX_DR      0x40
#define NRF_STATUS_TX_DS      0x20
#define NRF_STATUS_MAX_RT     0x10

/* FIFO_STATUS bits */
#define NRF_FIFO_RX_EMPTY     0x01

/* RF_SETUP bits */
#define NRF_RF_SETUP_DR_LOW   0x20
#define NRF_RF_SETUP_DR_HIGH  0x08

/* FEATURE bits */
#define NRF_FEATURE_EN_DPL    0x04

#define NRF_MAX_PAYLOAD       32
#define NRF_SPI_HZ            8000000UL
#define NRF_POWERUP_DELAY_MS  2      // Tpd2stby, spec maximum is 1.5 ms
#define NRF_TX_TIMEOUT_MS     100

/* ======================================================================= */
/* Constructors                                                             */
/* ======================================================================= */

NiusNRF24L01::NiusNRF24L01(uint8_t cePin, uint8_t csnPin) {
    _cePin          = cePin;
    _csnPin         = csnPin;
    _sckPin         = 0;
    _mosiPin        = 0;
    _misoPin        = 0;
    _softSPI        = false;
    _ready          = false;
    _spi            = &SPI;
    _plus           = false;
    _dynamicPayload = false;
    _listening      = false;
    _maxRetryFail   = false;
    _addrWidth      = 5;
    _payloadSize    = NRF_MAX_PAYLOAD;
}

NiusNRF24L01::NiusNRF24L01(uint8_t cePin, uint8_t csnPin, SPIClass &spi) {
    _cePin          = cePin;
    _csnPin         = csnPin;
    _sckPin         = 0;
    _mosiPin        = 0;
    _misoPin        = 0;
    _softSPI        = false;
    _ready          = false;
    _spi            = &spi;
    _plus           = false;
    _dynamicPayload = false;
    _listening      = false;
    _maxRetryFail   = false;
    _addrWidth      = 5;
    _payloadSize    = NRF_MAX_PAYLOAD;
}

NiusNRF24L01::NiusNRF24L01(uint8_t cePin, uint8_t csnPin,
                           uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin) {
    _cePin          = cePin;
    _csnPin         = csnPin;
    _sckPin         = sckPin;
    _mosiPin        = mosiPin;
    _misoPin        = misoPin;
    _softSPI        = true;
    _ready          = false;
    _spi            = NULL;
    _plus           = false;
    _dynamicPayload = false;
    _listening      = false;
    _maxRetryFail   = false;
    _addrWidth      = 5;
    _payloadSize    = NRF_MAX_PAYLOAD;
}

/* ======================================================================= */
/* Low-level SPI                                                            */
/* ======================================================================= */

uint8_t NiusNRF24L01::softTransfer(uint8_t data) {
    uint8_t received = 0;
    for (int8_t bit = 7; bit >= 0; --bit) {
        digitalWrite(_mosiPin, (data & (1 << bit)) ? HIGH : LOW);
        digitalWrite(_sckPin, HIGH);
        if (digitalRead(_misoPin)) received |= (uint8_t)(1 << bit);
        digitalWrite(_sckPin, LOW);
    }
    return received;
}

uint8_t NiusNRF24L01::spiTransfer(uint8_t data) {
    if (_softSPI) return softTransfer(data);
    return _spi->transfer(data);
}

void NiusNRF24L01::beginTransfer() {
    if (!_softSPI) _spi->beginTransaction(SPISettings(NRF_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(_csnPin, LOW);
}

void NiusNRF24L01::endTransfer() {
    digitalWrite(_csnPin, HIGH);
    if (!_softSPI) _spi->endTransaction();
}

void NiusNRF24L01::ceHigh() { digitalWrite(_cePin, HIGH); }
void NiusNRF24L01::ceLow()  { digitalWrite(_cePin, LOW); }

uint8_t NiusNRF24L01::readReg(uint8_t reg) {
    beginTransfer();
    spiTransfer(NRF_CMD_R_REGISTER | (reg & NRF_REGISTER_MASK));
    uint8_t value = spiTransfer(NRF_CMD_NOP);
    endTransfer();
    return value;
}

void NiusNRF24L01::writeReg(uint8_t reg, uint8_t val) {
    beginTransfer();
    spiTransfer(NRF_CMD_W_REGISTER | (reg & NRF_REGISTER_MASK));
    spiTransfer(val);
    endTransfer();
}

void NiusNRF24L01::readRegBuf(uint8_t reg, uint8_t *buf, uint8_t len) {
    beginTransfer();
    spiTransfer(NRF_CMD_R_REGISTER | (reg & NRF_REGISTER_MASK));
    for (uint8_t i = 0; i < len; ++i) buf[i] = spiTransfer(NRF_CMD_NOP);
    endTransfer();
}

void NiusNRF24L01::writeRegBuf(uint8_t reg, const uint8_t *buf, uint8_t len) {
    beginTransfer();
    spiTransfer(NRF_CMD_W_REGISTER | (reg & NRF_REGISTER_MASK));
    for (uint8_t i = 0; i < len; ++i) spiTransfer(buf[i]);
    endTransfer();
}

uint8_t NiusNRF24L01::sendCommand(uint8_t cmd) {
    beginTransfer();
    uint8_t status = spiTransfer(cmd);
    endTransfer();
    return status;
}

/* ======================================================================= */
/* NiusBase interface                                                       */
/* ======================================================================= */

bool NiusNRF24L01::begin() {
    pinMode(_cePin, OUTPUT);
    pinMode(_csnPin, OUTPUT);
    ceLow();
    digitalWrite(_csnPin, HIGH);

    if (_softSPI) {
        pinMode(_sckPin, OUTPUT);
        pinMode(_mosiPin, OUTPUT);
        pinMode(_misoPin, INPUT);
        digitalWrite(_sckPin, LOW);
        digitalWrite(_mosiPin, LOW);
    } else {
        _spi->begin();
    }

    delay(NRF_POWERUP_DELAY_MS + 3);   // allow the supply/oscillator to settle

    // Presence check: SETUP_AW is a small read/write register that must hold a
    // written value. A missing or unpowered module reads back 0x00 or 0xFF.
    writeReg(NRF_REG_SETUP_AW, 0x03);
    if (readReg(NRF_REG_SETUP_AW) != 0x03) {
        _ready = false;
        return false;
    }
    writeReg(NRF_REG_SETUP_AW, 0x01);
    if (readReg(NRF_REG_SETUP_AW) != 0x01) {
        _ready = false;
        return false;
    }
    writeReg(NRF_REG_SETUP_AW, 0x03);  // back to 5-byte addresses
    _addrWidth = 5;

    // Enhanced ShockBurst defaults: 1500 us retransmit delay, 15 retries.
    writeReg(NRF_REG_SETUP_RETR, 0x5F);
    writeReg(NRF_REG_EN_AA, 0x3F);
    writeReg(NRF_REG_EN_RXADDR, 0x03);

    // 250 kbps only exists on NRF24L01+ silicon; use it to identify the part.
    uint8_t rfSetup = readReg(NRF_REG_RF_SETUP);
    writeReg(NRF_REG_RF_SETUP, (uint8_t)(rfSetup | NRF_RF_SETUP_DR_LOW));
    _plus = (readReg(NRF_REG_RF_SETUP) & NRF_RF_SETUP_DR_LOW) != 0;
    writeReg(NRF_REG_RF_SETUP, rfSetup);

    setDataRate(NIUS_NRF24_1MBPS);
    setPower(NIUS_NRF24_PWR_MAX);
    setChannel(76);                    // clear of most WiFi traffic by default

    // Dynamic payload length. Older silicon and some clones need ACTIVATE
    // before FEATURE/DYNPD become writable.
    writeReg(NRF_REG_FEATURE, NRF_FEATURE_EN_DPL);
    if ((readReg(NRF_REG_FEATURE) & NRF_FEATURE_EN_DPL) == 0) {
        beginTransfer();
        spiTransfer(NRF_CMD_ACTIVATE);
        spiTransfer(0x73);
        endTransfer();
        writeReg(NRF_REG_FEATURE, NRF_FEATURE_EN_DPL);
    }
    if ((readReg(NRF_REG_FEATURE) & NRF_FEATURE_EN_DPL) != 0) {
        writeReg(NRF_REG_DYNPD, 0x3F);
        _dynamicPayload = (readReg(NRF_REG_DYNPD) & 0x3F) != 0;
    } else {
        _dynamicPayload = false;
    }
    if (!_dynamicPayload) {
        // Fixed-width fallback: every pipe carries a full 32-byte payload.
        for (uint8_t pipe = 0; pipe < 6; ++pipe) {
            writeReg((uint8_t)(NRF_REG_RX_PW_P0 + pipe), _payloadSize);
        }
    }

    reset();

    // Power up in standby-I with a 2-byte CRC.
    writeReg(NRF_REG_CONFIG,
             (uint8_t)(NRF_CONFIG_EN_CRC | NRF_CONFIG_CRCO | NRF_CONFIG_PWR_UP));
    delay(NRF_POWERUP_DELAY_MS);
    _listening = false;

    _ready = (readReg(NRF_REG_CONFIG) & NRF_CONFIG_PWR_UP) != 0;
    return _ready;
}

bool NiusNRF24L01::isReady() {
    if (!_ready) return false;
    // Re-prove the link to the chip rather than trusting a cached flag.
    return readReg(NRF_REG_SETUP_AW) == 0x03;
}

void NiusNRF24L01::reset() {
    sendCommand(NRF_CMD_FLUSH_TX);
    sendCommand(NRF_CMD_FLUSH_RX);
    writeReg(NRF_REG_STATUS,
             (uint8_t)(NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));
    _maxRetryFail = false;
}

String NiusNRF24L01::getVersion() {
    if (!_ready) return String("NRF24L01 (not detected)");
    return _plus ? String("NRF24L01+") : String("NRF24L01");
}

/* ======================================================================= */
/* Configuration                                                            */
/* ======================================================================= */

bool NiusNRF24L01::setChannel(uint8_t channel) {
    if (channel > 125) return false;
    writeReg(NRF_REG_RF_CH, channel);
    return readReg(NRF_REG_RF_CH) == channel;
}

bool NiusNRF24L01::setDataRate(uint8_t rate) {
    uint8_t setup = (uint8_t)(readReg(NRF_REG_RF_SETUP)
                              & ~(NRF_RF_SETUP_DR_LOW | NRF_RF_SETUP_DR_HIGH));
    switch (rate) {
        case NIUS_NRF24_250KBPS:
            if (!_plus) return false;               // not available on plain NRF24L01
            setup |= NRF_RF_SETUP_DR_LOW;
            break;
        case NIUS_NRF24_1MBPS:
            break;                                   // both rate bits clear
        case NIUS_NRF24_2MBPS:
            setup |= NRF_RF_SETUP_DR_HIGH;
            break;
        default:
            return false;
    }
    writeReg(NRF_REG_RF_SETUP, setup);
    return readReg(NRF_REG_RF_SETUP) == setup;
}

bool NiusNRF24L01::setPower(uint8_t level) {
    if (level > NIUS_NRF24_PWR_MAX) return false;
    uint8_t setup = (uint8_t)(readReg(NRF_REG_RF_SETUP) & ~0x06);
    setup |= (uint8_t)(level << 1);
    writeReg(NRF_REG_RF_SETUP, setup);
    return readReg(NRF_REG_RF_SETUP) == setup;
}

bool NiusNRF24L01::setAddress(uint8_t *addr, uint8_t len) {
    if (addr == NULL || len < 2 || len > 5) return false;
    // SETUP_AW encodes 3/4/5 bytes as 1/2/3; a 2-byte address is not supported
    // by the hardware, so the shortest usable width is 3.
    uint8_t width = (len < 3) ? 3 : len;
    writeReg(NRF_REG_SETUP_AW, (uint8_t)(width - 2));
    if (readReg(NRF_REG_SETUP_AW) != (uint8_t)(width - 2)) return false;
    _addrWidth = width;
    writeRegBuf(NRF_REG_RX_ADDR_P1, addr, width);
    writeReg(NRF_REG_EN_RXADDR, (uint8_t)(readReg(NRF_REG_EN_RXADDR) | 0x02));
    return true;
}

bool NiusNRF24L01::setAutoAck(bool enabled) {
    writeReg(NRF_REG_EN_AA, enabled ? 0x3F : 0x00);
    return readReg(NRF_REG_EN_AA) == (enabled ? 0x3F : 0x00);
}

bool NiusNRF24L01::setRetries(uint8_t delayStep, uint8_t count) {
    if (delayStep > 15 || count > 15) return false;
    uint8_t value = (uint8_t)((delayStep << 4) | count);
    writeReg(NRF_REG_SETUP_RETR, value);
    return readReg(NRF_REG_SETUP_RETR) == value;
}

/* ======================================================================= */
/* Pipe management                                                          */
/* ======================================================================= */

void NiusNRF24L01::openWritingPipe(uint8_t *addr) {
    if (addr == NULL) return;
    // Pipe 0 must mirror the destination address so auto-acknowledge frames
    // coming back from the receiver are accepted.
    writeRegBuf(NRF_REG_RX_ADDR_P0, addr, _addrWidth);
    writeRegBuf(NRF_REG_TX_ADDR, addr, _addrWidth);
    writeReg(NRF_REG_EN_RXADDR, (uint8_t)(readReg(NRF_REG_EN_RXADDR) | 0x01));
    if (!_dynamicPayload) writeReg(NRF_REG_RX_PW_P0, _payloadSize);
}

void NiusNRF24L01::openReadingPipe(uint8_t pipe, uint8_t *addr) {
    if (pipe > 5 || addr == NULL) return;
    if (pipe < 2) {
        // Pipes 0 and 1 carry a full-width address.
        writeRegBuf((uint8_t)(NRF_REG_RX_ADDR_P0 + pipe), addr, _addrWidth);
    } else {
        // Pipes 2..5 share pipe 1's high bytes and store only the LSB.
        writeRegBuf((uint8_t)(NRF_REG_RX_ADDR_P0 + pipe), addr, 1);
    }
    if (!_dynamicPayload) {
        writeReg((uint8_t)(NRF_REG_RX_PW_P0 + pipe), _payloadSize);
    }
    writeReg(NRF_REG_EN_RXADDR,
             (uint8_t)(readReg(NRF_REG_EN_RXADDR) | (uint8_t)(1 << pipe)));
}

/* ======================================================================= */
/* Data transfer                                                            */
/* ======================================================================= */

void NiusNRF24L01::startListening() {
    writeReg(NRF_REG_CONFIG,
             (uint8_t)(readReg(NRF_REG_CONFIG) | NRF_CONFIG_PWR_UP | NRF_CONFIG_PRIM_RX));
    writeReg(NRF_REG_STATUS,
             (uint8_t)(NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));
    sendCommand(NRF_CMD_FLUSH_RX);
    ceHigh();
    delayMicroseconds(150);            // Tstby2a
    _listening = true;
}

void NiusNRF24L01::stopListening() {
    ceLow();
    delayMicroseconds(150);
    sendCommand(NRF_CMD_FLUSH_TX);
    writeReg(NRF_REG_CONFIG,
             (uint8_t)((readReg(NRF_REG_CONFIG) & ~NRF_CONFIG_PRIM_RX) | NRF_CONFIG_PWR_UP));
    delayMicroseconds(150);
    _listening = false;
}

bool NiusNRF24L01::available() {
    if ((readReg(NRF_REG_STATUS) & NRF_STATUS_RX_DR) != 0) return true;
    return (readReg(NRF_REG_FIFO_STATUS) & NRF_FIFO_RX_EMPTY) == 0;
}

bool NiusNRF24L01::readRadio(uint8_t *buf, uint8_t len) {
    if (buf == NULL || len == 0 || len > NRF_MAX_PAYLOAD) return false;
    if (!available()) return false;

    uint8_t width = _payloadSize;
    if (_dynamicPayload) {
        beginTransfer();
        spiTransfer(NRF_CMD_R_RX_PL_WID);
        width = spiTransfer(NRF_CMD_NOP);
        endTransfer();
        if (width > NRF_MAX_PAYLOAD) {
            // A corrupt width must be discarded or the FIFO stalls.
            sendCommand(NRF_CMD_FLUSH_RX);
            writeReg(NRF_REG_STATUS, NRF_STATUS_RX_DR);
            return false;
        }
    }

    beginTransfer();
    spiTransfer(NRF_CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < width; ++i) {
        uint8_t value = spiTransfer(NRF_CMD_NOP);
        if (i < len) buf[i] = value;   // drain the whole payload either way
    }
    endTransfer();

    writeReg(NRF_REG_STATUS, NRF_STATUS_RX_DR);
    return true;
}

bool NiusNRF24L01::writeRadio(uint8_t *buf, uint8_t len) {
    if (buf == NULL || len == 0 || len > NRF_MAX_PAYLOAD) return false;

    bool wasListening = _listening;
    if (_listening) stopListening();

    sendCommand(NRF_CMD_FLUSH_TX);
    writeReg(NRF_REG_STATUS,
             (uint8_t)(NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT | NRF_STATUS_RX_DR));
    _maxRetryFail = false;

    uint8_t width = _dynamicPayload ? len : _payloadSize;
    beginTransfer();
    spiTransfer(NRF_CMD_W_TX_PAYLOAD);
    for (uint8_t i = 0; i < width; ++i) {
        spiTransfer(i < len ? buf[i] : 0x00);   // zero-pad in fixed-width mode
    }
    endTransfer();

    ceHigh();
    delayMicroseconds(15);             // >10 us starts the transmission
    ceLow();

    bool sent = false;
    uint32_t started = millis();
    for (;;) {
        uint8_t status = readReg(NRF_REG_STATUS);
        if (status & NRF_STATUS_TX_DS) { sent = true; break; }
        if (status & NRF_STATUS_MAX_RT) { _maxRetryFail = true; break; }
        if ((millis() - started) >= NRF_TX_TIMEOUT_MS) break;
    }

    writeReg(NRF_REG_STATUS,
             (uint8_t)(NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));
    if (!sent) sendCommand(NRF_CMD_FLUSH_TX);   // never leave a stuck packet

    if (wasListening) startListening();
    return sent;
}

/* ======================================================================= */
/* Diagnostics                                                              */
/* ======================================================================= */

uint8_t NiusNRF24L01::getStatus() {
    return sendCommand(NRF_CMD_NOP);
}

bool NiusNRF24L01::testCarrier() {
    // RPD latches while the receiver is on, so sample it in RX mode.
    bool wasListening = _listening;
    if (!wasListening) startListening();
    delayMicroseconds(200);
    bool detected = (readReg(NRF_REG_RPD) & 0x01) != 0;
    if (!wasListening) stopListening();
    return detected;
}
