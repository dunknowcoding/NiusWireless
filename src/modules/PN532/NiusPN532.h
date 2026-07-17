/*
 * NiusPN532.h — PN532 NFC / RFID module driver
 *
 * The PN532 supports ISO 14443A/B, ISO 18092 (NFC-IP1), MIFARE Classic,
 * MIFARE Ultralight, FeliCa and NDEF. It can communicate over I2C, SPI
 * or UART (HSU) — this driver supports I2C and SPI.
 *
 * I2C (4-wire):
 *   SDA, SCL — I2C bus (address 0x24, fixed)
 *   VCC, GND
 *   Pass irqPin = 0xFF and rstPin = 0xFF — IRQ / RSTO are SPI-mode pins.
 *
 * SPI:
 *   CS / SCK / MOSI / MISO
 *   IRQ, RSTO (reset) — used in SPI mode (see pn532_spi_* examples)
 *
 * --- Reference wiring (RobotDyn SAMD21 M0-Mini / Arduino Zero, I2C) ---
 *   PN532 SDA -> D20 / SDA
 *   PN532 SCL -> D21 / SCL
 *   PN532 VCC -> 3V3
 *   PN532 GND -> GND
 *   Set the module jumpers / DIP switches to I2C mode
 *   (typically SEL0=LOW, SEL1=HIGH — check your breakout).
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
     * For 4-wire I2C pass 0xFF, 0xFF (no IRQ / RSTO).
     * irqPin / rstPin are only used when those lines are actually wired
     * (SPI mode, or a breakout that brings them out alongside I2C).
     */
    NiusPN532(uint8_t irqPin, uint8_t rstPin);

    /**
     * I2C constructor with an explicit TwoWire bus (Wire / Wire1 / …).
     * For 4-wire I2C pass irqPin = 0xFF and rstPin = 0xFF.
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
     * setI2CClock() — Set the Wire bus clock in Hz (default 100000).
     * Call before begin(), or call again later to change the rate.
     */
    void setI2CClock(uint32_t hz);

    /**
     * setPassiveActivationRetries() — MxRtyPassiveActivation (0xFF = try
     * forever). Default after begin() is 0xFF.
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
     * Populates uid[] / uidLen / atqa / sak on success.
     */
    bool cardPresent();

    /** printInfo() — Print UID / ATQA / SAK to Serial. */
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

    /* -------------------------------------------------------------------
     * MIFARE Classic operations
     * ------------------------------------------------------------------ */

    /**
     * authenticate() — Authenticate a MIFARE Classic sector.
     * blockAddr — block number
     * keyType   — NIUS_KEY_A or NIUS_KEY_B
     * key       — 6-byte key
     * Returns NIUS_OK on success.
     */
    uint8_t authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key);

    /**
     * readBlock() — Read 16 bytes from a MIFARE Classic block.
     */
    uint8_t readBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * writeBlock() — Write 16 bytes to a MIFARE Classic block.
     */
    uint8_t writeBlock(uint8_t blockAddr, uint8_t *data);

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
    TwoWire *_i2c;

    void initCommon();

    bool    wakeup();
    bool    waitReady(uint16_t timeoutMs);
    bool    writeCommand(const uint8_t *cmd, uint8_t cmdLen);
    int16_t readResponse(uint8_t *buf, uint8_t bufLen, uint16_t timeoutMs);
    bool    sendCommandCheckAck(const uint8_t *cmd, uint8_t cmdLen, uint16_t timeoutMs);
    bool    readAck(uint16_t timeoutMs);
    bool    isReadyByte();
    bool    i2cReadBytes(uint8_t *buf, uint8_t n);

    void    spiBeginTxn();
    void    spiEndTxn();
    uint8_t spiTransfer(uint8_t data);

    bool    samConfig();
    uint8_t dataExchange(const uint8_t *send, uint8_t sendLen,
                         uint8_t *recv, uint8_t &recvLen);
    bool    readUltralightPage(uint8_t page, uint8_t *data4);
    bool    writeUltralightPage(uint8_t page, const uint8_t *data4);
};

#endif // NIUS_PN532_H
