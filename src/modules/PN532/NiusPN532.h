/*
 * NiusPN532.h — PN532 NFC / RFID module driver
 *
 * The PN532 supports ISO 14443A/B, ISO 18092 (NFC-IP1), MIFARE Classic,
 * MIFARE Ultralight, FeliCa and NDEF. It can communicate over I2C, SPI
 * or UART (HSU) — this driver supports I2C and SPI.
 *
 * I2C (default):
 *   SDA, SCL — I2C bus (address 0x24, fixed)
 *   IRQ      — interrupt from PN532, active LOW (recommended)
 *   RST      — reset, active LOW
 *
 * SPI:
 *   CS   — chip-select
 *   SCK, MOSI, MISO
 *   IRQ, RST
 *
 * --- Status: STUB — full implementation coming in a future release ---
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

/* =======================================================================
 * NiusPN532 class
 * ====================================================================== */
class NiusPN532 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * I2C constructor (default).
     * irqPin — interrupt pin (GPIO connected to PN532 IRQ, active LOW)
     * rstPin — reset pin
     */
    NiusPN532(uint8_t irqPin, uint8_t rstPin);

    /**
     * SPI constructor.
     * csPin  — chip-select
     * rstPin — reset pin
     * The last parameter useSPI must be true to distinguish from I2C.
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
     * Card / tag detection
     * ------------------------------------------------------------------ */

    /**
     * cardPresent() — Scan for an ISO 14443A card in the field.
     * Populates uid[] and uidLen on success.
     * Returns true if a card was found.
     */
    bool cardPresent();

    /**
     * getUID() — Return the UID of the last detected card as a hex String.
     */
    String getUID();

    /**
     * getUIDBytes() — Copy UID bytes into buf.
     * buf — at least 10 bytes
     * len — set to UID length (4, 7 or 10)
     * Returns true if a UID is available.
     */
    bool getUIDBytes(uint8_t *buf, uint8_t &len);

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
     * blockAddr — authenticated block number
     * data      — 16-byte output buffer
     * Returns NIUS_OK on success.
     */
    uint8_t readBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * writeBlock() — Write 16 bytes to a MIFARE Classic block.
     * blockAddr — authenticated block number
     * data      — 16-byte input buffer
     * Returns NIUS_OK on success.
     */
    uint8_t writeBlock(uint8_t blockAddr, uint8_t *data);

    /* -------------------------------------------------------------------
     * NDEF operations (MIFARE Ultralight / NFC Type 2 tags)
     * ------------------------------------------------------------------ */

    /**
     * readNDEF() — Read NDEF message from a Type 2 tag.
     * buf — output buffer (at least NIUS_PN532_NDEF_MAX bytes)
     * len — set to the number of bytes read
     * Returns true on success.
     */
    bool readNDEF(uint8_t *buf, uint8_t &len);

    /**
     * writeNDEF() — Write an NDEF message to a Type 2 tag.
     * buf — NDEF message bytes
     * len — number of bytes to write
     * Returns true on success.
     */
    bool writeNDEF(uint8_t *buf, uint8_t len);

    /* -------------------------------------------------------------------
     * Public state
     * ------------------------------------------------------------------ */
    uint8_t uid[10];
    uint8_t uidLen;

private:
    uint8_t _irqPin;
    uint8_t _rstPin;
    uint8_t _csPin;
    bool    _useSPI;
    bool    _ready;
};

#endif // NIUS_PN532_H
