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
static const uint8_t PN532_CMD_INCOMMUNICATETHRU  = 0x42;
static const uint8_t PN532_CMD_INDATAEXCHANGE       = 0x40;

/* SPI status / R/W opcodes */
static const uint8_t PN532_SPI_STATREAD  = 0x02;
static const uint8_t PN532_SPI_DATAWRITE = 0x01;
static const uint8_t PN532_SPI_DATAREAD  = 0x03;
static const uint8_t PN532_SPI_READY     = 0x01;

static const uint8_t PN532_I2C_READY = 0x01;

static const uint8_t PN532_CMD_DIAGNOSE        = 0x00;
static const uint8_t PN532_CMD_READREGISTER    = 0x06;
static const uint8_t PN532_CMD_WRITEREGISTER   = 0x08;
static const uint8_t PN532_CMD_SETPARAMETERS   = 0x12;
static const uint8_t PN532_CMD_INRELEASE       = 0x52;
static const uint8_t PN532_CMD_POWERDOWN       = 0x16;
static const uint8_t PN532_CMD_INAUTOPOLL      = 0x60;

#ifndef NIUS_PN532_SPI_SCK
#define NIUS_PN532_SPI_SCK   24
#endif
#ifndef NIUS_PN532_SPI_MOSI
#define NIUS_PN532_SPI_MOSI  23
#endif
#ifndef NIUS_PN532_SPI_MISO
#define NIUS_PN532_SPI_MISO  22
#endif


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
    _i2cAddr  = NIUS_PN532_I2C_ADDR;
    _mxRtyPassive = 0x20;
    uidLen       = 0;
    lastError    = NIUS_ERR_UNKNOWN;
    lastCardType = NIUS_CARD_UNKNOWN;
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
#if defined(ARDUINO_ARCH_SAMD)
        pinMode(NIUS_PN532_SPI_SCK, OUTPUT);
        digitalWrite(NIUS_PN532_SPI_SCK, LOW);
        pinMode(NIUS_PN532_SPI_MOSI, OUTPUT);
        digitalWrite(NIUS_PN532_SPI_MOSI, LOW);
        pinMode(NIUS_PN532_SPI_MISO, INPUT_PULLUP);
#else
        SPI.begin();
#endif
    } else {
#if defined(ARDUINO_ARCH_SAMD)
        /*
         * If SPI header is still attached (NSS/SCK/MOSI/MISO), idle NSS high
         * and float the SPI GPIO so they cannot hold the chip out of I2C.
         * Reference wiring uses SS=D8 and ICSP 24/23/22.
         */
        pinMode(8, OUTPUT);
        digitalWrite(8, HIGH);
        pinMode(NIUS_PN532_SPI_SCK, INPUT);
        pinMode(NIUS_PN532_SPI_MOSI, INPUT);
        pinMode(NIUS_PN532_SPI_MISO, INPUT);
        /* External pull-ups on the module; enable pads before SERCOM claim. */
        pinMode(SDA, INPUT_PULLUP);
        pinMode(SCL, INPUT_PULLUP);
        delay(1);
#endif
        _i2c->begin();
        _i2c->setClock(_i2cClock);
#if defined(WIRE_HAS_TIMEOUT)
        /* PN532 clock-stretches; give SAMD Wire enough time. */
        _i2c->setWireTimeout(25000, true);
#endif
        delay(50); /* oscillator / interface settle after host reset */
    }

    reset();
    delay(_rstPin != 0xFF ? 50 : 10);
    wakeup();
    delay(10);

    /* Adafruit SPI: dummy GetFirmwareVersion to sync host/PN532. */
    if (_useSPI) {
        uint8_t syncCmd[] = {PN532_CMD_GETFIRMWAREVERSION};
        if (sendCommandCheckAck(syncCmd, 1, 1000)) {
            uint8_t dump[16];
            (void)readResponse(dump, sizeof(dump), 200);
        }
        delay(10);
    } else {
        if (_irqPin != 0xFF) {
            if (!drainIrqResponse()) {
                _irqPin = 0xFF; /* fall back to I2C status polling */
            }
        }

        /* Prefer 0x24; accept common alternates if they ACK. Stay on I2C only. */
        _i2cAddr = NIUS_PN532_I2C_ADDR;
        {
            uint8_t candidates[] = { 0x24, 0x48, 0x42, 0x4E };
            for (uint8_t i = 0; i < sizeof(candidates); i++) {
                _i2c->beginTransmission(candidates[i]);
                if (_i2c->endTransmission() == 0) {
                    _i2cAddr = candidates[i];
                    break;
                }
            }
        }

        /* Adafruit-style: attempt GetFW even if address probe NACKed. */
        uint8_t syncCmd[] = {PN532_CMD_GETFIRMWAREVERSION};
        if (sendCommandCheckAck(syncCmd, 1, 1500)) {
            uint8_t dump[16];
            (void)readResponse(dump, sizeof(dump), 500);
        }
        delay(10);
    }

    uint32_t ver = 0;
    if (!getFirmwareVersion(ver)) {
        delay(50);
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

    (void)setPassiveActivationRetries(0x20);

    if (!samConfig()) {
        delay(50);
        if (!samConfig()) {
            return false;
        }
    }
    delay(20);

    (void)applyTypeAAnalog();

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

void NiusPN532::setIRQPin(uint8_t irqPin) {
    _irqPin = irqPin;
    if (_irqPin != 0xFF) {
        pinMode(_irqPin, INPUT_PULLUP);
    }
}

bool NiusPN532::setRFField(uint8_t autoRFCA, uint8_t rfOn) {
    uint8_t cmd[] = {
        PN532_CMD_RFCONFIGURATION,
        0x01,
        (uint8_t)((rfOn ? 0x01 : 0x00) | (autoRFCA ? 0x02 : 0x00))
    };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[4];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_RFCONFIGURATION + 1));
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
    if (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_RFCONFIGURATION + 1)) {
        _mxRtyPassive = maxRetries;
        return true;
    }
    return false;
}

bool NiusPN532::applyTypeAAnalog() {
    /*
     * Type-A 106 analog via RFConfiguration 0x0A. On this SAMD USB supply,
     * factory CWGsP=0x3F browns out / kills RX; CWGsP=0x12 reads UIDs reliably.
     * Must be set with RFConfiguration (not only WriteRegister) so InList uses it.
     */
    uint8_t ana[] = {
        PN532_CMD_RFCONFIGURATION, 0x0A,
        0x59, 0xF4, 0x12, 0x11, 0x4D, 0x84, 0x61, 0x6F, 0x26, 0x62, 0x87
    };
    if (!sendCommandCheckAck(ana, sizeof(ana), 500)) {
        return false;
    }
    uint8_t r[4];
    return readResponse(r, sizeof(r), 300) >= 1;
}

/* -----------------------------------------------------------------------
 * Card detection
 * ---------------------------------------------------------------------- */

bool NiusPN532::cardPresent() {
    lastError = NIUS_ERR_UNKNOWN;
    lastCardType = NIUS_CARD_UNKNOWN;
    if (!_ready) {
        lastError = NIUS_ERR_PARAM;
        return false;
    }

    /* InList reloads default analog; re-apply CWGsP=0x12 every scan. */
    (void)applyTypeAAnalog();

    uint8_t cmd[] = {
        PN532_CMD_INLISTPASSIVETARGET,
        0x01,
        0x00
    };

    /* Empty-field wait scales with MxRtyPassiveActivation. */
    uint16_t tmo = 200;
    if (_mxRtyPassive == 0xFF) {
        tmo = 1000;
    } else if (_mxRtyPassive > 4) {
        tmo = (uint16_t)(100 + (uint16_t)_mxRtyPassive * 25);
        if (tmo > 1000) {
            tmo = 1000;
        }
    }

    if (!sendCommandCheckAck(cmd, sizeof(cmd), tmo)) {
        lastError = NIUS_ERR_TIMEOUT;
        return false;
    }

    uint8_t resp[32];
    int16_t n = readResponse(resp, sizeof(resp), tmo);
    if (n < 2) {
        lastError = NIUS_ERR_TIMEOUT;
        return false;
    }
    if (resp[0] != (uint8_t)(PN532_CMD_INLISTPASSIVETARGET + 1)) {
        lastError = NIUS_ERR_UNKNOWN;
        return false;
    }
    if (resp[1] < 1 || n < 7) {
        lastError = NIUS_ERR_NOTAG;
        return false;
    }

    _tg   = resp[2];
    _atqa = ((uint16_t)resp[3] << 8) | resp[4];
    _sak  = resp[5];
    uint8_t uidLength = resp[6];
    if (uidLength == 0 || uidLength > 10 || (int16_t)(7 + uidLength) > n) {
        lastError = NIUS_ERR_OVERFLOW;
        return false;
    }

    uidLen = uidLength;
    memcpy(uid, &resp[7], uidLen);
    lastCardType = sakToCardType(_sak);
    lastError = NIUS_OK;
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
    NIUS_SERIAL.print(F("Type: "));
    NIUS_SERIAL.println(getCardTypeName());
}

String NiusPN532::getCardTypeName() const {
    switch (lastCardType) {
        case NIUS_CARD_MIFARE_MINI:  return String(F("MIFARE Mini"));
        case NIUS_CARD_MIFARE_1K:    return String(F("MIFARE Classic 1K"));
        case NIUS_CARD_MIFARE_4K:    return String(F("MIFARE Classic 4K"));
        case NIUS_CARD_MIFARE_UL:    return String(F("MIFARE Ultralight"));
        case NIUS_CARD_MIFARE_PLUS:  return String(F("MIFARE Plus"));
        case NIUS_CARD_ISO14443_4:   return String(F("ISO 14443-4"));
        case NIUS_CARD_ISO18092:     return String(F("ISO 18092 (NFC-IP1)"));
        case NIUS_CARD_TNP3XXX:      return String(F("TNP3xxx"));
        case NIUS_CARD_DESFIRE:      return String(F("MIFARE DESFire"));
        default:                     return String(F("Unknown"));
    }
}

const __FlashStringHelper *NiusPN532::errorName(uint8_t code) {
    switch (code) {
        case NIUS_OK:            return F("OK");
        case NIUS_ERR_NOTAG:     return F("NOTAG");
        case NIUS_ERR_TIMEOUT:   return F("TIMEOUT");
        case NIUS_ERR_CRC:       return F("CRC");
        case NIUS_ERR_COLLISION: return F("COLLISION");
        case NIUS_ERR_AUTH:      return F("AUTH");
        case NIUS_ERR_OVERFLOW:  return F("OVERFLOW");
        case NIUS_ERR_PARAM:     return F("PARAM");
        default:                 return F("UNKNOWN");
    }
}

uint8_t NiusPN532::sakToCardType(uint8_t sak) const {
    if (sak & 0x20) {
        return NIUS_CARD_ISO14443_4;
    }
    if (sak & 0x40) {
        return NIUS_CARD_ISO18092;
    }
    switch (sak & 0x7F) {
        case 0x00: return NIUS_CARD_MIFARE_UL;
        case 0x09: return NIUS_CARD_MIFARE_MINI;
        case 0x08: return NIUS_CARD_MIFARE_1K;
        case 0x18: return NIUS_CARD_MIFARE_4K;
        case 0x10:
        case 0x11: return NIUS_CARD_MIFARE_PLUS;
        default:   return NIUS_CARD_UNKNOWN;
    }
}

uint8_t NiusPN532::mapPn532Status(uint8_t status) const {
    switch (status) {
        case 0x00: return NIUS_OK;
        case 0x01: return NIUS_ERR_TIMEOUT;
        case 0x02: return NIUS_ERR_CRC;
        case 0x03: return NIUS_ERR_CRC;       /* parity */
        case 0x04: return NIUS_ERR_UNKNOWN;   /* bitcount */
        case 0x05: return NIUS_ERR_UNKNOWN;   /* framing */
        case 0x06: return NIUS_ERR_COLLISION;
        case 0x0A: return NIUS_ERR_OVERFLOW;
        case 0x13: return NIUS_ERR_OVERFLOW;  /* buffer overflow */
        case 0x14: return NIUS_ERR_AUTH;
        default:   return NIUS_ERR_UNKNOWN;
    }
}

bool NiusPN532::classicBlockOk(uint8_t blockAddr) const {
    if (lastCardType == NIUS_CARD_MIFARE_4K) {
        return blockAddr <= 255;
    }
    if (lastCardType == NIUS_CARD_MIFARE_1K ||
        lastCardType == NIUS_CARD_MIFARE_MINI) {
        return blockAddr <= 63;
    }
    /* Unknown / not yet typed: allow Classic 1K range only. */
    return blockAddr <= 63;
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
    if (!_ready || uidLen < 4) {
        return NIUS_ERR_PARAM;
    }
    if (keyType != NIUS_KEY_A && keyType != NIUS_KEY_B) {
        return NIUS_ERR_PARAM;
    }
    if (!key) {
        key = (uint8_t *)NIUS_KEY_DEFAULT;
    }
    if (!classicBlockOk(blockAddr)) {
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
    if (!classicBlockOk(blockAddr)) {
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

uint8_t NiusPN532::writeBlock(uint8_t blockAddr, uint8_t *data, bool force) {
    if (!_ready || !data) {
        return NIUS_ERR_PARAM;
    }
    if (!classicBlockOk(blockAddr)) {
        return NIUS_ERR_PARAM;
    }

    /* Sensitive-block guard — same policy as NiusRC522. */
    if (!force) {
        if (blockAddr == 0) {
            return NIUS_ERR_PARAM; /* use setUid() */
        }
        if ((blockAddr & 0x03) == 0x03) {
            return NIUS_ERR_PARAM; /* sector trailer */
        }
    }

    uint8_t send[18];
    send[0] = 0xA0;
    send[1] = blockAddr;
    memcpy(&send[2], data, 16);

    uint8_t recv[8];
    uint8_t recvLen = sizeof(recv);
    return dataExchange(send, 18, recv, recvLen);
}

uint8_t NiusPN532::setUid(uint8_t *newUid, uint8_t uidSize, bool commit) {
    if (!newUid || (uidSize != 4 && uidSize != 7)) {
        return NIUS_ERR_PARAM;
    }
    if (lastCardType != NIUS_CARD_MIFARE_1K &&
        lastCardType != NIUS_CARD_MIFARE_MINI &&
        lastCardType != NIUS_CARD_MIFARE_4K) {
        return NIUS_ERR_PARAM;
    }

    /* Sector 0 auth first — many cards refuse bare block-0 reads. */
    uint8_t r = authenticate(3, NIUS_KEY_A, nullptr);
    if (r != NIUS_OK) {
        r = authenticate(3, NIUS_KEY_B, nullptr);
    }
    if (r != NIUS_OK) {
        return NIUS_ERR_AUTH;
    }

    uint8_t origBlock0[16];
    r = readBlock(0, origBlock0);
    if (r != NIUS_OK) {
        stopCrypto();
        return r;
    }

    uint8_t newBlock0[16];
    memcpy(newBlock0, origBlock0, 16);
    memcpy(newBlock0, newUid, uidSize);

    uint8_t bcc = 0;
    for (uint8_t i = 0; i < uidSize; i++) {
        bcc ^= newUid[i];
    }
    newBlock0[uidSize] = bcc;
    /* Bytes uidSize+1..15 preserved (manufacturer / padding). */

    NIUS_SERIAL.println(F("--- setUid preview ---"));
    NIUS_SERIAL.print(F("  Card:      "));
    NIUS_SERIAL.println(getCardTypeName());
    NIUS_SERIAL.print(F("  Old UID:   "));
    for (uint8_t i = 0; i < uidSize; i++) {
        if (origBlock0[i] < 0x10) NIUS_SERIAL.print('0');
        NIUS_SERIAL.print(origBlock0[i], HEX);
    }
    NIUS_SERIAL.println();
    NIUS_SERIAL.print(F("  New UID:   "));
    for (uint8_t i = 0; i < uidSize; i++) {
        if (newUid[i] < 0x10) NIUS_SERIAL.print('0');
        NIUS_SERIAL.print(newUid[i], HEX);
    }
    NIUS_SERIAL.println();
    NIUS_SERIAL.print(F("  Old BCC:   0x"));
    if (origBlock0[uidSize] < 0x10) NIUS_SERIAL.print('0');
    NIUS_SERIAL.println(origBlock0[uidSize], HEX);
    NIUS_SERIAL.print(F("  New BCC:   0x"));
    if (bcc < 0x10) NIUS_SERIAL.print('0');
    NIUS_SERIAL.println(bcc, HEX);
    NIUS_SERIAL.print(F("  Mfr bytes: "));
    for (uint8_t i = (uint8_t)(uidSize + 1); i <= 7; i++) {
        if (origBlock0[i] < 0x10) NIUS_SERIAL.print('0');
        NIUS_SERIAL.print(origBlock0[i], HEX);
        if (i < 7) NIUS_SERIAL.print(' ');
    }
    NIUS_SERIAL.println(F("  (preserved)"));

    if (!commit) {
        NIUS_SERIAL.println(F("  DRY-RUN: pass commit=true to write (CUID/magic only)."));
        return NIUS_OK;
    }

    if (authenticate(3, NIUS_KEY_A, nullptr) != NIUS_OK &&
        authenticate(3, NIUS_KEY_B, nullptr) != NIUS_OK) {
        NIUS_SERIAL.println(F("  ERROR: Sector 0 auth failed."));
        return NIUS_ERR_AUTH;
    }

    r = writeBlock(0, newBlock0, true);
    stopCrypto();
    if (r != NIUS_OK) {
        NIUS_SERIAL.println(F("  ERROR: writeBlock(0) failed."));
        return r;
    }

    halt();
    if (!cardPresent()) {
        NIUS_SERIAL.println(F("  ERROR: Card not re-detected after UID write."));
        return NIUS_ERR_UNKNOWN;
    }
    bool uidMatches = (uidLen == uidSize);
    for (uint8_t i = 0; i < uidSize && uidMatches; i++) {
        if (uid[i] != newUid[i]) {
            uidMatches = false;
        }
    }
    if (!uidMatches) {
        NIUS_SERIAL.println(F("  ERROR: UID mismatch after write."));
        return NIUS_ERR_UNKNOWN;
    }
    NIUS_SERIAL.println(F("  OK: UID changed and verified."));
    return NIUS_OK;
}

void NiusPN532::stopCrypto() {
    if (_ready && _tg) {
        (void)inRelease(_tg);
    }
}

void NiusPN532::halt() {
    stopCrypto();
    uidLen = 0;
    lastCardType = NIUS_CARD_UNKNOWN;
}

uint8_t NiusPN532::readPage(uint8_t page, uint8_t *data) {
    if (!_ready || !data) {
        return NIUS_ERR_PARAM;
    }
    return readUltralightPage(page, data) ? NIUS_OK : NIUS_ERR_UNKNOWN;
}

uint8_t NiusPN532::writePage(uint8_t page, uint8_t *data) {
    if (!_ready || !data) {
        return NIUS_ERR_PARAM;
    }
    /* Refuse UID / lock pages by default (0..3). */
    if (page < 4) {
        return NIUS_ERR_PARAM;
    }
    return writeUltralightPage(page, data) ? NIUS_OK : NIUS_ERR_UNKNOWN;
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

bool NiusPN532::diagnose(uint8_t numTst, const uint8_t *in, uint8_t inLen,
                               uint8_t *out, uint8_t &outLen) {
    uint8_t cmd[16];
    if (inLen > 12) {
        return false;
    }
    cmd[0] = PN532_CMD_DIAGNOSE;
    cmd[1] = numTst;
    for (uint8_t i = 0; i < inLen; i++) {
        cmd[2 + i] = in[i];
    }
    if (!sendCommandCheckAck(cmd, (uint8_t)(2 + inLen), 2000)) {
        return false;
    }
    uint8_t resp[32];
    int16_t n = readResponse(resp, sizeof(resp), 2000);
    if (n < 1 || resp[0] != (uint8_t)(PN532_CMD_DIAGNOSE + 1)) {
        return false;
    }
    /* Payload after cmd byte: for 0x07 antenna, typically NumTst + Result. */
    uint8_t payload = (uint8_t)(n - 1);
    if (payload > outLen) {
        payload = outLen;
    }
    if (out && payload) {
        memcpy(out, &resp[1], payload);
    }
    outLen = payload;
    return true;
}

bool NiusPN532::setParameters(uint8_t flags) {
    uint8_t cmd[] = { PN532_CMD_SETPARAMETERS, flags };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[4];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_SETPARAMETERS + 1));
}

bool NiusPN532::writeRegister(uint16_t reg, uint8_t value) {
    uint8_t cmd[] = {
        PN532_CMD_WRITEREGISTER,
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        value
    };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[4];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_WRITEREGISTER + 1));
}

bool NiusPN532::readRegister(uint16_t reg, uint8_t &value) {
    uint8_t cmd[] = {
        PN532_CMD_READREGISTER,
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF)
    };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[8];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    if (n < 2 || resp[0] != (uint8_t)(PN532_CMD_READREGISTER + 1)) {
        return false;
    }
    value = resp[1];
    return true;
}

bool NiusPN532::inRelease(uint8_t tg) {
    uint8_t cmd[] = { PN532_CMD_INRELEASE, tg };
    if (!sendCommandCheckAck(cmd, sizeof(cmd), 1000)) {
        return false;
    }
    uint8_t resp[8];
    int16_t n = readResponse(resp, sizeof(resp), 1000);
    return (n >= 1 && resp[0] == (uint8_t)(PN532_CMD_INRELEASE + 1));
}

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
    /* SPI: status-byte ready (useIRQ=0). I2C: useIRQ=1 when IRQ wired. */
    uint8_t useIrq = (!_useSPI && _irqPin != 0xFF) ? 0x01 : 0x00;
    uint8_t cmd[] = {
        PN532_CMD_SAMCONFIGURATION,
        0x01,
        0x14,
        useIrq
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
        return mapPn532Status(resp[1]);
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

/* If P70_IRQ is stuck LOW, unread host data is pending — drain until idle. */
bool NiusPN532::drainIrqResponse() {
    if (_useSPI || _irqPin == 0xFF) {
        return true;
    }
    uint8_t rounds = 0;
    while (digitalRead(_irqPin) == LOW && rounds < 16) {
        rounds++;
        uint8_t avail = (uint8_t)_i2c->requestFrom((int)_i2cAddr, 32);
        for (uint8_t i = 0; i < avail; i++) {
            (void)_i2c->read();
        }
        delay(5);
    }
    return (digitalRead(_irqPin) == HIGH);
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
        uint8_t avail = (uint8_t)_i2c->requestFrom((int)_i2cAddr, (int)req);
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

    uint8_t got = (uint8_t)_i2c->requestFrom((int)_i2cAddr, 1);
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
            if (digitalRead(_irqPin) == LOW) {
                return true;
            }
        } else if (isReadyByte()) {
            return true;
        }
        delay(10);
    }
    return false;
}

bool NiusPN532::waitReadyStatus(uint16_t timeoutMs) {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        if (isReadyByte()) {
            return true;
        }
        delay(10);
    }
    return false;
}

bool NiusPN532::waitIrqHigh(uint16_t timeoutMs) {
    if (_irqPin == 0xFF) {
        return true;
    }
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        if (digitalRead(_irqPin) == HIGH) {
            return true;
        }
        delay(10);
    }
    return false;
}

bool NiusPN532::waitReadyData(uint16_t timeoutMs) {
    return _useSPI ? waitReadyStatus(timeoutMs) : waitReady(timeoutMs);
}

bool NiusPN532::readAck(uint16_t timeoutMs) {
    bool ready = (_useSPI || _irqPin == 0xFF) ? waitReadyStatus(timeoutMs)
                                              : waitReady(timeoutMs);
    if (!ready) {
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

bool NiusPN532::readAckBytes(uint8_t *ack) {
    return readAck(1000) && ack != nullptr;
}

bool NiusPN532::sendCommandCheckAck(const uint8_t *cmd, uint8_t cmdLen,
                                    uint16_t timeoutMs) {
    /* I2C+IRQ: drain any pending frame so stuck LOW does not fake ready. */
    if (!_useSPI && _irqPin != 0xFF) {
        (void)drainIrqResponse();
        (void)waitIrqHigh(50); /* best-effort idle; do not hard-fail */
    }
    if (!writeCommand(cmd, cmdLen)) {
        return false;
    }
    delay(2);
    if (!readAck(timeoutMs)) {
        return false;
    }
    delay(2);
    if (_useSPI) {
        return waitReadyStatus(timeoutMs);
    }
    if (_irqPin != 0xFF) {
        return waitReady(timeoutMs);
    }
    return waitReadyStatus(timeoutMs);
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
        spiBeginTxnWake();
        spiTransfer(PN532_SPI_DATAWRITE);
        for (uint8_t i = 0; i < n; i++) {
            spiTransfer(packet[i]);
        }
        spiEndTxn();
        return true;
    }

    _i2c->beginTransmission(_i2cAddr);
    _i2c->write(packet, n);
    uint8_t et = _i2c->endTransmission();
    return (et == 0);
}

int16_t NiusPN532::readResponse(uint8_t *buf, uint8_t bufLen, uint16_t timeoutMs) {
    bool ready = (_useSPI || _irqPin == 0xFF) ? waitReadyStatus(timeoutMs)
                                              : waitReady(timeoutMs);
    if (!ready) {
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
        uint8_t avail = (uint8_t)_i2c->requestFrom((int)_i2cAddr,
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
#if defined(ARDUINO_ARCH_SAMD)
    digitalWrite(_csPin, LOW);
#else
    SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
#endif
}

void NiusPN532::spiBeginTxnWake() {
#if defined(ARDUINO_ARCH_SAMD)
    digitalWrite(_csPin, LOW);
    delay(2);
#else
    SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
    delay(2);
#endif
}

void NiusPN532::spiEndTxn() {
    digitalWrite(_csPin, HIGH);
#if !defined(ARDUINO_ARCH_SAMD)
    SPI.endTransaction();
#endif
}

#if defined(ARDUINO_ARCH_SAMD)
uint8_t NiusPN532::softSpiTransfer(uint8_t data) {
    uint8_t indata = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
        digitalWrite(NIUS_PN532_SPI_MOSI, (data & 0x01) ? HIGH : LOW);
        data = (uint8_t)(data >> 1);
        delayMicroseconds(2);
        digitalWrite(NIUS_PN532_SPI_SCK, HIGH);
        delayMicroseconds(2);
        indata = (uint8_t)(indata >> 1);
        if (digitalRead(NIUS_PN532_SPI_MISO)) {
            indata |= 0x80;
        }
        digitalWrite(NIUS_PN532_SPI_SCK, LOW);
        delayMicroseconds(2);
    }
    return indata;
}
#endif

uint8_t NiusPN532::spiTransfer(uint8_t data) {
#if defined(ARDUINO_ARCH_SAMD)
    return softSpiTransfer(data);
#else
    return SPI.transfer(data);
#endif
}
