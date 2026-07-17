/*
 * NiusPN532.cpp — PN532 NFC / RFID driver (I2C + SPI)
 *
 * Implements the PN532 host protocol (GetFirmwareVersion, SAMConfiguration,
 * InListPassiveTarget, InDataExchange) for ISO 14443A card detection and
 * basic MIFARE Classic / Type-2 NDEF operations.
 */

#include "NiusPN532.h"
#include <string.h>

/* PN532 frame markers */
static const uint8_t PN532_PREAMBLE      = 0x00;
static const uint8_t PN532_STARTCODE1    = 0x00;
static const uint8_t PN532_STARTCODE2    = 0xFF;
static const uint8_t PN532_POSTAMBLE     = 0x00;
static const uint8_t PN532_HOST_TO_PN532 = 0xD4;
static const uint8_t PN532_PN532_TO_HOST = 0xD5;

/* PN532 commands */
static const uint8_t PN532_CMD_GETFIRMWAREVERSION   = 0x02;
static const uint8_t PN532_CMD_SAMCONFIGURATION     = 0x14;
static const uint8_t PN532_CMD_RFCONFIGURATION      = 0x32;
static const uint8_t PN532_CMD_INLISTPASSIVETARGET  = 0x4A;
static const uint8_t PN532_CMD_INDATAEXCHANGE       = 0x40;

/* SPI status / R/W opcodes */
static const uint8_t PN532_SPI_STATREAD  = 0x02;
static const uint8_t PN532_SPI_DATAWRITE = 0x01;
static const uint8_t PN532_SPI_DATAREAD  = 0x03;
static const uint8_t PN532_SPI_READY     = 0x01;

static const uint8_t PN532_I2C_READY = 0x01;

static const uint8_t PN532_ACK[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

/* Keep I2C reads within the smallest common Wire buffer (AVR = 32). */
static const uint8_t PN532_I2C_CHUNK = 32;

/* -----------------------------------------------------------------------
 * Constructors
 * ---------------------------------------------------------------------- */

void NiusPN532::initCommon() {
    _ready    = false;
    _fwVer    = 0;
    _fwRev    = 0;
    _tg       = 1;
    _atqa     = 0;
    _sak      = 0;
    _i2cClock = 100000UL;
    uidLen    = 0;
    memset(uid, 0, sizeof(uid));
}

NiusPN532::NiusPN532(uint8_t irqPin, uint8_t rstPin) {
    _irqPin = irqPin;
    _rstPin = rstPin;
    _csPin  = 0;
    _useSPI = false;
    _i2c    = &Wire;
    initCommon();
}

NiusPN532::NiusPN532(TwoWire &bus, uint8_t irqPin, uint8_t rstPin) {
    _irqPin = irqPin;
    _rstPin = rstPin;
    _csPin  = 0;
    _useSPI = false;
    _i2c    = &bus;
    initCommon();
}

NiusPN532::NiusPN532(uint8_t csPin, uint8_t rstPin, bool useSPI) {
    _csPin  = csPin;
    _rstPin = rstPin;
    _irqPin = 0xFF;
    _useSPI = useSPI;
    _i2c    = &Wire;
    initCommon();
}

/* -----------------------------------------------------------------------
 * NiusBase
 * ---------------------------------------------------------------------- */

bool NiusPN532::begin() {
    _ready = false;
    uidLen = 0;

    if (_rstPin != 0xFF) {
        pinMode(_rstPin, OUTPUT);
        digitalWrite(_rstPin, HIGH);
    }
    if (_irqPin != 0xFF) {
        pinMode(_irqPin, INPUT_PULLUP);
    }

    if (_useSPI) {
        pinMode(_csPin, OUTPUT);
        digitalWrite(_csPin, HIGH);
        SPI.begin();
    } else {
#if defined(ARDUINO_ARCH_SAMD)
        /* If a previous transaction left SDA low, SAMD Wire can hang on the
         * next endTransmission. Clock SCL and issue a STOP to free the bus. */
        pinMode(SDA, INPUT_PULLUP);
        pinMode(SCL, OUTPUT);
        for (uint8_t i = 0; i < 9; i++) {
            digitalWrite(SCL, HIGH);
            delayMicroseconds(5);
            if (digitalRead(SDA) == HIGH) {
                break;
            }
            digitalWrite(SCL, LOW);
            delayMicroseconds(5);
        }
        digitalWrite(SCL, LOW);
        delayMicroseconds(5);
        pinMode(SDA, OUTPUT);
        digitalWrite(SDA, LOW);
        delayMicroseconds(5);
        digitalWrite(SCL, HIGH);
        delayMicroseconds(5);
        pinMode(SDA, INPUT_PULLUP);   // SDA rises = STOP
        delayMicroseconds(5);
#endif
        _i2c->begin();
        _i2c->setClock(_i2cClock);
#if defined(WIRE_HAS_TIMEOUT)
        _i2c->setWireTimeout(3000, true);
#endif
    }

    reset();
    delay(10);
    /* I2C: Adafruit wakes the PN532 by running SAMConfig (clock stretch).
     * Do not poke the bus with a bare endTransmission — on SAMD that can
     * hang forever if SDA/SCL are stuck. */
    wakeup();

    if (!samConfig()) {
        delay(50);
        if (!samConfig()) {
            return false;
        }
    }

    uint32_t ver = 0;
    if (!getFirmwareVersion(ver)) {
        delay(20);
        if (!getFirmwareVersion(ver)) {
            return false;
        }
    }

    uint8_t ic = (uint8_t)(ver >> 24);
    if (ic != 0x32) {
        return false;
    }
    _fwVer = (uint8_t)(ver >> 16);
    _fwRev = (uint8_t)(ver >> 8);

    setPassiveActivationRetries(0xFF);

    _ready = true;
    return true;
}

bool NiusPN532::isReady() {
    return _ready;
}

void NiusPN532::reset() {
    if (_rstPin == 0xFF) {
        return;
    }
    /* Datasheet Fig.48 — brief low pulse, then settle */
    digitalWrite(_rstPin, HIGH);
    delay(10);
    digitalWrite(_rstPin, LOW);
    delay(1);
    digitalWrite(_rstPin, HIGH);
    delay(2);
}

String NiusPN532::getVersion() {
    if (!_ready && _fwVer == 0) {
        return String("PN532 (not ready)");
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "PN532 v%u.%u", (unsigned)_fwVer, (unsigned)_fwRev);
    return String(buf);
}

void NiusPN532::setI2CClock(uint32_t hz) {
    _i2cClock = hz ? hz : 100000UL;
    if (!_useSPI && _i2c) {
        _i2c->setClock(_i2cClock);
    }
}

bool NiusPN532::setPassiveActivationRetries(uint8_t maxRetries) {
    uint8_t cmd[] = {
        PN532_CMD_RFCONFIGURATION,
        0x05,             // Config item: MaxRetries
        0xFF,             // MxRtyATR
        0x01,             // MxRtyPSL
        maxRetries        // MxRtyPassiveActivation
    };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[4];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_RFCONFIGURATION + 1));
}

/* -----------------------------------------------------------------------
 * Card detection
 * ---------------------------------------------------------------------- */

bool NiusPN532::cardPresent() {
    if (!_ready) {
        return false;
    }

    uint8_t cmd[] = {
        PN532_CMD_INLISTPASSIVETARGET,
        0x01,   // max targets
        0x00    // 106 kbps Type A
    };

    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }

    uint8_t resp[32];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    if (n < 7) {
        return false;
    }
    if (resp[0] != (uint8_t)(PN532_CMD_INLISTPASSIVETARGET + 1)) {
        return false;
    }
    if (resp[1] < 1) {
        return false;
    }

    _tg   = resp[2];
    _atqa = ((uint16_t)resp[3] << 8) | resp[4];
    _sak  = resp[5];
    uint8_t uidLength = resp[6];
    if (uidLength == 0 || uidLength > 10 || (int16_t)(7 + uidLength) > n) {
        return false;
    }

    uidLen = uidLength;
    memcpy(uid, &resp[7], uidLen);
    return true;
}

void NiusPN532::printInfo() {
    NIUS_SERIAL.print(F("UID: "));
    NIUS_SERIAL.println(getUID());
    NIUS_SERIAL.print(F("ATQA: 0x"));
    if (_atqa < 0x1000) NIUS_SERIAL.print('0');
    if (_atqa < 0x100)  NIUS_SERIAL.print('0');
    if (_atqa < 0x10)   NIUS_SERIAL.print('0');
    NIUS_SERIAL.println(_atqa, HEX);
    NIUS_SERIAL.print(F("SAK:  0x"));
    if (_sak < 0x10) NIUS_SERIAL.print('0');
    NIUS_SERIAL.println(_sak, HEX);
}

String NiusPN532::getUID() {
    if (uidLen == 0) {
        return String("");
    }
    char hex[32];
    uint8_t pos = 0;
    for (uint8_t i = 0; i < uidLen && pos + 3 < sizeof(hex); i++) {
        if (i > 0) {
            hex[pos++] = ' ';
        }
        static const char digits[] = "0123456789ABCDEF";
        hex[pos++] = digits[(uid[i] >> 4) & 0x0F];
        hex[pos++] = digits[uid[i] & 0x0F];
    }
    hex[pos] = '\0';
    return String(hex);
}

bool NiusPN532::getUIDBytes(uint8_t *buf, uint8_t &len) {
    if (!buf || uidLen == 0) {
        len = 0;
        return false;
    }
    memcpy(buf, uid, uidLen);
    len = uidLen;
    return true;
}

/* -----------------------------------------------------------------------
 * MIFARE Classic
 * ---------------------------------------------------------------------- */

uint8_t NiusPN532::authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key) {
    if (!_ready || !key || uidLen < 4) {
        return NIUS_ERR_PARAM;
    }
    if (keyType != NIUS_KEY_A && keyType != NIUS_KEY_B) {
        return NIUS_ERR_PARAM;
    }

    /* Auth frame: keyType, block, key[6], uid[uidLen] — same as Adafruit */
    uint8_t send[2 + 6 + 10];
    uint8_t n = 0;
    send[n++] = keyType;
    send[n++] = blockAddr;
    memcpy(&send[n], key, 6);
    n = (uint8_t)(n + 6);
    memcpy(&send[n], uid, uidLen);
    n = (uint8_t)(n + uidLen);

    uint8_t recv[16];
    uint8_t recvLen = sizeof(recv);
    return dataExchange(send, n, recv, recvLen);
}

uint8_t NiusPN532::readBlock(uint8_t blockAddr, uint8_t *data) {
    if (!_ready || !data) {
        return NIUS_ERR_PARAM;
    }

    uint8_t send[2] = {0x30, blockAddr};
    uint8_t recv[18];
    uint8_t recvLen = sizeof(recv);
    uint8_t st = dataExchange(send, 2, recv, recvLen);
    if (st != NIUS_OK) {
        return st;
    }
    if (recvLen < 16) {
        return NIUS_ERR_UNKNOWN;
    }
    memcpy(data, recv, 16);
    return NIUS_OK;
}

uint8_t NiusPN532::writeBlock(uint8_t blockAddr, uint8_t *data) {
    if (!_ready || !data) {
        return NIUS_ERR_PARAM;
    }

    uint8_t send[18];
    send[0] = 0xA0;
    send[1] = blockAddr;
    memcpy(&send[2], data, 16);

    uint8_t recv[8];
    uint8_t recvLen = sizeof(recv);
    return dataExchange(send, 18, recv, recvLen);
}

/* -----------------------------------------------------------------------
 * Type-2 NDEF (Ultralight / NTAG)
 * ---------------------------------------------------------------------- */

bool NiusPN532::readUltralightPage(uint8_t page, uint8_t *data4) {
    uint8_t send[2] = {0x30, page};
    uint8_t recv[18];
    uint8_t recvLen = sizeof(recv);
    if (dataExchange(send, 2, recv, recvLen) != NIUS_OK || recvLen < 4) {
        return false;
    }
    memcpy(data4, recv, 4);
    return true;
}

bool NiusPN532::writeUltralightPage(uint8_t page, const uint8_t *data4) {
    uint8_t send[6];
    send[0] = 0xA2;
    send[1] = page;
    memcpy(&send[2], data4, 4);
    uint8_t recv[8];
    uint8_t recvLen = sizeof(recv);
    return dataExchange(send, 6, recv, recvLen) == NIUS_OK;
}

bool NiusPN532::readNDEF(uint8_t *buf, uint8_t &len) {
    if (!_ready || !buf) {
        len = 0;
        return false;
    }

    uint8_t scratch[16];
    uint8_t send[2] = {0x30, 4};
    uint8_t recvLen = sizeof(scratch);
    if (dataExchange(send, 2, scratch, recvLen) != NIUS_OK || recvLen < 16) {
        len = 0;
        return false;
    }

    if (scratch[0] != 0x03) {
        len = 0;
        return false;
    }

    uint16_t ndefLen = 0;
    uint8_t hdr = 2;
    if (scratch[1] == 0xFF) {
        ndefLen = ((uint16_t)scratch[2] << 8) | scratch[3];
        hdr = 4;
    } else {
        ndefLen = scratch[1];
    }
    if (ndefLen == 0 || ndefLen > NIUS_PN532_NDEF_MAX) {
        len = 0;
        return false;
    }

    uint16_t total = (uint16_t)hdr + ndefLen;
    uint8_t first = (total < 16) ? (uint8_t)total : 16;
    memcpy(buf, &scratch[hdr], (size_t)(first - hdr));
    uint16_t copied = (uint16_t)(first - hdr);

    uint8_t page = 8;
    while (copied < ndefLen) {
        send[1] = page;
        recvLen = sizeof(scratch);
        if (dataExchange(send, 2, scratch, recvLen) != NIUS_OK || recvLen < 16) {
            len = 0;
            return false;
        }
        uint16_t remain = (uint16_t)(ndefLen - copied);
        uint8_t take = (remain < 16) ? (uint8_t)remain : 16;
        memcpy(&buf[copied], scratch, take);
        copied = (uint16_t)(copied + take);
        page = (uint8_t)(page + 4);
    }

    len = (uint8_t)ndefLen;
    return true;
}

bool NiusPN532::writeNDEF(uint8_t *buf, uint8_t len) {
    if (!_ready || !buf || len == 0 || len > (NIUS_PN532_NDEF_MAX - 2)) {
        return false;
    }

    uint8_t tlv[NIUS_PN532_NDEF_MAX + 4];
    uint16_t tlvLen = 0;
    tlv[tlvLen++] = 0x03;
    tlv[tlvLen++] = len;
    memcpy(&tlv[tlvLen], buf, len);
    tlvLen = (uint16_t)(tlvLen + len);
    tlv[tlvLen++] = 0xFE;

    while (tlvLen % 4) {
        tlv[tlvLen++] = 0x00;
    }

    uint8_t page = 4;
    for (uint16_t i = 0; i < tlvLen; i = (uint16_t)(i + 4)) {
        if (!writeUltralightPage(page, &tlv[i])) {
            return false;
        }
        page++;
    }
    return true;
}

/* -----------------------------------------------------------------------
 * High-level command helpers
 * ---------------------------------------------------------------------- */

bool NiusPN532::getFirmwareVersion(uint32_t &version) {
    uint8_t cmd[] = {PN532_CMD_GETFIRMWAREVERSION};
    if (!sendCommandCheckAck(cmd, 1, 1000)) {
        return false;
    }
    uint8_t resp[8];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    if (n < 5 || resp[0] != (uint8_t)(PN532_CMD_GETFIRMWAREVERSION + 1)) {
        return false;
    }
    version = ((uint32_t)resp[1] << 24) |
              ((uint32_t)resp[2] << 16) |
              ((uint32_t)resp[3] << 8)  |
              (uint32_t)resp[4];
    return true;
}

bool NiusPN532::samConfig() {
    /* Normal mode, timeout = 50ms * 0x14 = 1s; IRQ enable follows wiring */
    uint8_t cmd[] = {
        PN532_CMD_SAMCONFIGURATION,
        0x01,
        0x14,
        (uint8_t)((_irqPin != 0xFF) ? 0x01 : 0x00)
    };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[8];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_SAMCONFIGURATION + 1));
}

uint8_t NiusPN532::dataExchange(const uint8_t *send, uint8_t sendLen,
                                uint8_t *recv, uint8_t &recvLen) {
    if (!send || sendLen == 0 || sendLen > (NIUS_PN532_FRAME_MAX - 8)) {
        return NIUS_ERR_PARAM;
    }

    uint8_t cmd[NIUS_PN532_FRAME_MAX];
    cmd[0] = PN532_CMD_INDATAEXCHANGE;
    cmd[1] = _tg;
    memcpy(&cmd[2], send, sendLen);

    if (!sendCommandCheckAck(cmd, (uint8_t)(2 + sendLen), 1000)) {
        return NIUS_ERR_TIMEOUT;
    }

    uint8_t resp[NIUS_PN532_FRAME_MAX];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    if (n < 2) {
        return NIUS_ERR_TIMEOUT;
    }
    if (resp[0] != (uint8_t)(PN532_CMD_INDATAEXCHANGE + 1)) {
        return NIUS_ERR_UNKNOWN;
    }
    if (resp[1] != 0x00) {
        if (resp[1] == 0x14) {
            return NIUS_ERR_AUTH;
        }
        return NIUS_ERR_UNKNOWN;
    }

    uint8_t payload = (uint8_t)(n - 2);
    if (payload > recvLen) {
        payload = recvLen;
    }
    if (recv && payload) {
        memcpy(recv, &resp[2], payload);
    }
    recvLen = payload;
    return NIUS_OK;
}

/* -----------------------------------------------------------------------
 * Wake / ready / ACK
 * ---------------------------------------------------------------------- */

bool NiusPN532::wakeup() {
    if (_useSPI) {
        digitalWrite(_csPin, LOW);
        delay(2);
        digitalWrite(_csPin, HIGH);
        delay(2);
        return true;
    }

    /* I2C: no bus poke here. PN532 wakes on the next host frame / SAMConfig
     * (it may clock-stretch). A naked endTransmission() can hang SAMD Wire. */
    delay(2);
    return true;
}

bool NiusPN532::i2cReadBytes(uint8_t *buf, uint8_t n) {
    /* status byte + n data bytes; chunked to fit Wire RX buffers */
    uint8_t got = 0;
    while (got < n) {
        uint8_t need = (uint8_t)(n - got);
        uint8_t chunk = need;
        if (chunk > (uint8_t)(PN532_I2C_CHUNK - 1)) {
            chunk = (uint8_t)(PN532_I2C_CHUNK - 1);
        }
        uint8_t req = (uint8_t)(chunk + 1);   // + status
        uint8_t avail = (uint8_t)_i2c->requestFrom((int)NIUS_PN532_I2C_ADDR, (int)req);
        if (avail < 2) {
            return false;
        }
        (void)_i2c->read();                   // status
        for (uint8_t i = 0; i < chunk && _i2c->available(); i++) {
            buf[got++] = (uint8_t)_i2c->read();
        }
    }
    return (got == n);
}

bool NiusPN532::isReadyByte() {
    if (_useSPI) {
        spiBeginTxn();
        spiTransfer(PN532_SPI_STATREAD);
        uint8_t st = spiTransfer(0x00);
        spiEndTxn();
        return (st & PN532_SPI_READY) != 0;
    }

    uint8_t got = (uint8_t)_i2c->requestFrom((int)NIUS_PN532_I2C_ADDR, 1);
    if (got == 0 || !_i2c->available()) {
        return false;
    }
    uint8_t st = (uint8_t)_i2c->read();
    return (st & PN532_I2C_READY) != 0;
}

bool NiusPN532::waitReady(uint16_t timeoutMs) {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        if (_irqPin != 0xFF) {
            /* IRQ low is a hint; always confirm with the status byte */
            if (digitalRead(_irqPin) == LOW && isReadyByte()) {
                return true;
            }
        }
        if (isReadyByte()) {
            return true;
        }
        delay(2);
    }
    return false;
}

bool NiusPN532::readAck(uint16_t timeoutMs) {
    if (!waitReady(timeoutMs)) {
        return false;
    }

    uint8_t ack[6];
    if (_useSPI) {
        spiBeginTxn();
        spiTransfer(PN532_SPI_DATAREAD);
        for (uint8_t i = 0; i < 6; i++) {
            ack[i] = spiTransfer(0x00);
        }
        spiEndTxn();
    } else if (!i2cReadBytes(ack, 6)) {
        return false;
    }

    return (memcmp(ack, PN532_ACK, 6) == 0);
}

bool NiusPN532::sendCommandCheckAck(const uint8_t *cmd, uint8_t cmdLen,
                                    uint16_t timeoutMs) {
    if (!writeCommand(cmd, cmdLen)) {
        return false;
    }
    /* Settle, read ACK, then wait until the response frame is ready
     * (same two-phase ready wait as Adafruit_PN532::sendCommandCheckAck). */
    delay(1);
    if (!readAck(timeoutMs)) {
        return false;
    }
    delay(1);
    return waitReady(timeoutMs);
}

/* -----------------------------------------------------------------------
 * Frame write / read
 * ---------------------------------------------------------------------- */

bool NiusPN532::writeCommand(const uint8_t *cmd, uint8_t cmdLen) {
    uint8_t packet[NIUS_PN532_FRAME_MAX];
    uint8_t len = (uint8_t)(cmdLen + 1);
    uint8_t lcs = (uint8_t)(~len + 1);
    uint8_t sum = PN532_HOST_TO_PN532;
    for (uint8_t i = 0; i < cmdLen; i++) {
        sum = (uint8_t)(sum + cmd[i]);
    }
    uint8_t dcs = (uint8_t)(~sum + 1);

    uint8_t n = 0;
    packet[n++] = PN532_PREAMBLE;
    packet[n++] = PN532_STARTCODE1;
    packet[n++] = PN532_STARTCODE2;
    packet[n++] = len;
    packet[n++] = lcs;
    packet[n++] = PN532_HOST_TO_PN532;
    for (uint8_t i = 0; i < cmdLen; i++) {
        packet[n++] = cmd[i];
    }
    packet[n++] = dcs;
    packet[n++] = PN532_POSTAMBLE;

    if (_useSPI) {
        spiBeginTxn();
        spiTransfer(PN532_SPI_DATAWRITE);
        for (uint8_t i = 0; i < n; i++) {
            spiTransfer(packet[i]);
        }
        spiEndTxn();
        return true;
    }

    _i2c->beginTransmission(NIUS_PN532_I2C_ADDR);
    _i2c->write(packet, n);
    return (_i2c->endTransmission() == 0);
}

int16_t NiusPN532::readResponse(uint8_t *buf, uint8_t bufLen, uint16_t timeoutMs) {
    if (!waitReady(timeoutMs)) {
        return -1;
    }

    uint8_t frame[NIUS_PN532_FRAME_MAX];
    uint8_t frameLen = 0;

    if (_useSPI) {
        spiBeginTxn();
        spiTransfer(PN532_SPI_DATAREAD);
        /* Clock a generous fixed-size frame; parser finds 00 FF below. */
        frameLen = 40;
        for (uint8_t i = 0; i < frameLen; i++) {
            frame[i] = spiTransfer(0x00);
        }
        spiEndTxn();
    } else {
        /* status + body; all responses we use fit in a 32-byte Wire buffer */
        uint8_t avail = (uint8_t)_i2c->requestFrom((int)NIUS_PN532_I2C_ADDR,
                                                   (int)PN532_I2C_CHUNK);
        if (avail < 7) {
            return -1;
        }
        (void)_i2c->read();   // status
        while (_i2c->available() && frameLen < NIUS_PN532_FRAME_MAX) {
            frame[frameLen++] = (uint8_t)_i2c->read();
        }
    }

    /* Locate 0x00 0xFF start code */
    int16_t start = -1;
    for (uint8_t i = 0; i + 1 < frameLen; i++) {
        if (frame[i] == PN532_STARTCODE1 && frame[i + 1] == PN532_STARTCODE2) {
            start = i;
            break;
        }
    }
    if (start < 0 || (uint8_t)(start + 5) > frameLen) {
        return -1;
    }

    uint8_t len = frame[start + 2];
    uint8_t lcs = frame[start + 3];
    if ((uint8_t)(len + lcs) != 0 || len < 2) {
        return -1;
    }

    uint8_t needEnd = (uint8_t)(start + 4 + len + 1);   // index of DCS
    if (needEnd >= frameLen) {
        return -1;
    }

    if (frame[start + 4] != PN532_PN532_TO_HOST) {
        return -1;
    }

    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + frame[start + 4 + i]);
    }
    if ((uint8_t)(sum + frame[start + 4 + len]) != 0) {
        return -1;
    }

    uint8_t payloadLen = (uint8_t)(len - 1);
    if (payloadLen > bufLen) {
        payloadLen = bufLen;
    }
    memcpy(buf, &frame[start + 5], payloadLen);
    return (int16_t)payloadLen;
}

/* -----------------------------------------------------------------------
 * SPI helpers
 * ---------------------------------------------------------------------- */

void NiusPN532::spiBeginTxn() {
    SPI.beginTransaction(SPISettings(1000000UL, LSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
}

void NiusPN532::spiEndTxn() {
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

uint8_t NiusPN532::spiTransfer(uint8_t data) {
    return SPI.transfer(data);
}
