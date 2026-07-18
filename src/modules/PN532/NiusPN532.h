/*
 * NiusPN532.h — PN532 NFC / RFID module driver
 *
 * The PN532 supports ISO 14443A/B, ISO 18092 (NFC-IP1), MIFARE Classic,
 * MIFARE Ultralight, FeliCa and NDEF. It can communicate over I2C, SPI
 * or UART (HSU) — this driver supports I2C and SPI.
 *
 * I2C:
 *   SDA, SCL — I2C bus (address 0x24, fixed)
 *   VCC, GND
 *   irqPin — strongly recommended on SAMD21 (Wire has no stretch timeout).
 *            Pass 0xFF only on boards whose Wire supports setWireTimeout().
 *   rstPin — optional RSTO reset (0xFF if unwired).
 *
 * SPI:
 *   CS / SCK / MOSI / MISO
 *   IRQ, RSTO (reset) — used in SPI mode (see pn532_spi_* examples)
 *
 * --- Reference wiring (RobotDyn SAMD21 M0-Mini / Arduino Zero, I2C) ---
 *   PN532 SDA -> D20 / SDA
 *   PN532 SCL -> D21 / SCL
 *   PN532 IRQ -> D9  (recommended on SAMD21)
 *   PN532 VCC -> 3V3
 *   PN532 GND -> GND
 */

#ifndef NIUS_PN532_H
#define NIUS_PN532_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "../../NiusBase.h"

/* -----------------------------------------------------------------------
 * PN532 I2C address (fixed, cannot be changed)
 * ---------------------------------------------------------------------- */
#define NIUS_PN532_I2C_ADDR  0x24

/* -----------------------------------------------------------------------
 * Maximum NDEF payload buffer size
 * ---------------------------------------------------------------------- */
#define NIUS_PN532_NDEF_MAX  128

/* -----------------------------------------------------------------------
 * Key-type constants for authenticate() (same values as MIFARE Classic)
 * ---------------------------------------------------------------------- */
#ifndef NIUS_KEY_A
#define NIUS_KEY_A  0x60
#endif
#ifndef NIUS_KEY_B
#define NIUS_KEY_B  0x61
#endif

/* Factory key (defined in NiusRC522.cpp; link via NiusWireless.h) */
extern const uint8_t NIUS_KEY_DEFAULT[NIUS_KEY_DEFAULT_LEN];

/* -----------------------------------------------------------------------
 * Frame / buffer sizes
 * ---------------------------------------------------------------------- */
#define NIUS_PN532_FRAME_MAX  64

/* =======================================================================
 * NiusPN532 class
 * ====================================================================== */
class NiusPN532 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * I2C constructor (default Wire bus).
     * For I2C pass the IRQ pin when wired (recommended on SAMD21).
     * Pass irqPin = 0xFF only if Wire has setWireTimeout() on your core.
     * Pass rstPin = 0xFF when RSTO is unwired.
     */
    NiusPN532(uint8_t irqPin, uint8_t rstPin);

    /**
     * I2C constructor with an explicit TwoWire bus (Wire / Wire1 / …).
     * For I2C pass irqPin when wired; 0xFF only with Wire timeout support.
     * Pass rstPin = 0xFF when RSTO is unwired.
     */
    NiusPN532(TwoWire &bus, uint8_t irqPin, uint8_t rstPin);

    /**
     * SPI constructor.
     * csPin  — chip-select (NSS)
     * rstPin — RSTO reset pin
     * The last parameter useSPI must be true to distinguish from I2C.
     * Wire IRQ separately when using interrupt-driven SPI examples.
     */
    NiusPN532(uint8_t csPin, uint8_t rstPin, bool useSPI);

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the PN532.
     * Returns true if the chip responds to GetFirmwareVersion, false otherwise.
     */
    bool begin();

    /** isReady() — Returns true if the module is initialised. */
    bool isReady();

    /** reset() — Perform a hardware reset via the RST pin. */
    void reset();

    /**
     * getVersion() — Returns firmware version string, e.g. "PN532 v1.6".
     */
    String getVersion();

    /* -------------------------------------------------------------------
     * I2C / RF configuration (advanced)
     * ------------------------------------------------------------------ */

    /**
     * setIRQPin() — Optional P70_IRQ pin (active LOW). Call before begin().
     * Used by I2C on SAMD21 and by SPI advanced examples. Pass 0xFF to
     * disable and fall back to status-byte polling.
     */
    void setIRQPin(uint8_t irqPin);

    /**
     * setI2CClock() — Set the Wire bus clock in Hz (default 100000).
     * Call before begin(), or call again later to change the rate.
     */
    void setI2CClock(uint32_t hz);

    /**
     * setRFField() — RFConfiguration item 1 (AutoRFCA / RF on-off).
     */
    bool setRFField(uint8_t autoRFCA, uint8_t rfOn);

    /**
     * setPassiveActivationRetries() — MxRtyPassiveActivation (0xFF = try
     * forever). Default after begin() is 0x20; advanced examples often use
     * 0x01–0x02 for faster empty-field polling.
     */
    bool setPassiveActivationRetries(uint8_t maxRetries);

    /**
     * getFirmwareVersion() — Raw IC / Ver / Rev / Support packing:
     *   (IC << 24) | (Ver << 16) | (Rev << 8) | Support
     * IC is 0x32 for PN532.
     */
    bool getFirmwareVersion(uint32_t &version);

    /* -------------------------------------------------------------------
     * Card / tag detection
     * ------------------------------------------------------------------ */

    /**
     * cardPresent() — Scan for an ISO 14443A card in the field.
     * Populates uid[] / uidLen / ATQA / SAK / lastCardType on success.
     * On failure sets lastError to NIUS_ERR_NOTAG / TIMEOUT / COLLISION / …
     */
    bool cardPresent();

    /** printInfo() — Print UID / ATQA / SAK / Type to Serial. */
    void printInfo();

    /** getUID() — UID of the last detected card as a hex String. */
    String getUID();

    /**
     * getUIDBytes() — Copy UID bytes into buf.
     * buf — at least 10 bytes
     * len — set to UID length (4, 7 or 10)
     */
    bool getUIDBytes(uint8_t *buf, uint8_t &len);

    uint16_t getATQA() const { return _atqa; }
    uint8_t  getSAK()  const { return _sak; }

    /** getCardType() — NIUS_CARD_* of the last successful detection. */
    uint8_t getCardType() const { return lastCardType; }

    /** getCardTypeName() — Human-readable name for getCardType(). */
    String getCardTypeName() const;

    /**
     * errorName() — Flash string for a NIUS_OK / NIUS_ERR_* code
     * (detection and MIFARE return codes).
     */
    static const __FlashStringHelper *errorName(uint8_t code);

    /* -------------------------------------------------------------------
     * MIFARE Classic operations
     * ------------------------------------------------------------------ */

    /**
     * authenticate() — Authenticate a MIFARE Classic sector.
     * blockAddr — block number
     * keyType   — NIUS_KEY_A or NIUS_KEY_B
     * key       — 6-byte key (nullptr → NIUS_KEY_DEFAULT)
     * Returns NIUS_OK on success.
     */
    uint8_t authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key);

    /**
     * readBlock() — Read 16 bytes from a MIFARE Classic block.
     */
    uint8_t readBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * writeBlock() — Write 16 bytes to a MIFARE Classic block.
     * By default refuses block 0 (UID/manufacturer) and sector trailers
     * (blocks 3,7,11,…). Pass force=true only from a vetted path such as
     * setUid() or an explicit trailer rewrite that preserves access bits.
     */
    uint8_t writeBlock(uint8_t blockAddr, uint8_t *data, bool force = false);

    /**
     * setUid() — Build / optionally commit a MIFARE Classic block-0 UID
     * change. Always recomputes BCC = XOR(UID) and preserves manufacturer
     * bytes from the current block 0. With commit=false (default) only
     * prints a preview. Requires a magic/CUID card for commit=true.
     */
    uint8_t setUid(uint8_t *newUid, uint8_t uidSize, bool commit = false);

    /** stopCrypto() — End the current MIFARE crypto session (InRelease). */
    void stopCrypto();

    /** halt() — Release the active target so the next scan can re-select. */
    void halt();

    /* -------------------------------------------------------------------
     * MIFARE Ultralight / NTAG page ops
     * ------------------------------------------------------------------ */

    /** readPage() — Read 4 bytes starting at page. */
    uint8_t readPage(uint8_t page, uint8_t *data);

    /** writePage() — Write 4 bytes to a page (avoid UID pages 0–1). */
    uint8_t writePage(uint8_t page, uint8_t *data);

    /* -------------------------------------------------------------------
     * NDEF operations (MIFARE Ultralight / NFC Type 2 tags)
     * ------------------------------------------------------------------ */

    bool readNDEF(uint8_t *buf, uint8_t &len);
    bool writeNDEF(uint8_t *buf, uint8_t len);

    /* -------------------------------------------------------------------
     * Public state
     * ------------------------------------------------------------------ */
    uint8_t uid[10];
    uint8_t uidLen;
    uint8_t lastError;       // last cardPresent() failure reason (or NIUS_OK)
    uint8_t lastCardType;    // NIUS_CARD_* after successful detection

private:
    uint8_t  _irqPin;
    uint8_t  _rstPin;
    uint8_t  _csPin;
    bool     _useSPI;
    bool     _ready;
    uint8_t  _fwVer;
    uint8_t  _fwRev;
    uint8_t  _tg;
    uint16_t _atqa;
    uint8_t  _sak;
    uint32_t _i2cClock;
    uint8_t  _i2cAddr;
    TwoWire *_i2c;
    uint8_t  _mxRtyPassive;

    void initCommon();

    bool    applyTypeAAnalog();
    uint8_t sakToCardType(uint8_t sak) const;
    uint8_t mapPn532Status(uint8_t status) const;
    bool    classicBlockOk(uint8_t blockAddr) const;

    bool    wakeup();
    bool    drainIrqResponse();
    bool    waitReady(uint16_t timeoutMs);
    bool    waitReadyStatus(uint16_t timeoutMs);
    bool    waitIrqHigh(uint16_t timeoutMs);
    bool    waitReadyData(uint16_t timeoutMs);
    bool    writeCommand(const uint8_t *cmd, uint8_t cmdLen);
    int16_t readResponse(uint8_t *buf, uint8_t bufLen, uint16_t timeoutMs);
    bool    sendCommandCheckAck(const uint8_t *cmd, uint8_t cmdLen, uint16_t timeoutMs);
    bool    readAck(uint16_t timeoutMs);
    bool    readAckBytes(uint8_t *ack);
    bool    isReadyByte();
    bool    i2cReadBytes(uint8_t *buf, uint8_t n);

    void    spiBeginTxn();
    void    spiBeginTxnWake();
    void    spiEndTxn();
    uint8_t spiTransfer(uint8_t data);
#if defined(ARDUINO_ARCH_SAMD)
    uint8_t softSpiTransfer(uint8_t data);
#endif

    bool    inRelease(uint8_t tg);
    bool    samConfig();
    bool    diagnose(uint8_t numTst, const uint8_t *in, uint8_t inLen,
                     uint8_t *out, uint8_t &outLen);
    bool    setParameters(uint8_t flags);
    bool    writeRegister(uint16_t reg, uint8_t value);
    bool    readRegister(uint16_t reg, uint8_t &value);
    uint8_t dataExchange(const uint8_t *send, uint8_t sendLen,
                         uint8_t *recv, uint8_t &recvLen);
    bool    readUltralightPage(uint8_t page, uint8_t *data4);
    bool    writeUltralightPage(uint8_t page, const uint8_t *data4);
};

#endif // NIUS_PN532_H
