/*
 * NiusNRF24L01.h — NRF24L01 / NRF24L01+ 2.4 GHz radio module driver
 *
 * The NRF24L01 communicates over SPI (Mode 0, MSB first, max 8 MHz).
 * Hardware SPI or software SPI is supported (see constructors).
 *
 * Pinout (typical):
 *   CE   — chip enable (activates RX/TX)
 *   CSN  — chip-select (SPI slave select, active LOW)
 *   SCK  — SPI clock
 *   MOSI — SPI data in
 *   MISO — SPI data out
 *   IRQ  — interrupt (optional, active LOW)
 *
 * Dynamic payload length is enabled automatically on NRF24L01+ silicon (and on
 * clones that accept the ACTIVATE command).  When the chip does not support it
 * the driver falls back to a fixed 32-byte payload width, so writeRadio() and
 * readRadio() behave the same either way.
 */

#ifndef NIUS_NRF24L01_H
#define NIUS_NRF24L01_H

#include <Arduino.h>
#include <SPI.h>
#include "../../NiusBase.h"

/* -----------------------------------------------------------------------
 * Data-rate constants for setDataRate()
 * ---------------------------------------------------------------------- */
#define NIUS_NRF24_250KBPS  0
#define NIUS_NRF24_1MBPS    1
#define NIUS_NRF24_2MBPS    2

/* -----------------------------------------------------------------------
 * TX power constants for setPower()
 * ---------------------------------------------------------------------- */
#define NIUS_NRF24_PWR_MIN  0  // -18 dBm
#define NIUS_NRF24_PWR_LOW  1  // -12 dBm
#define NIUS_NRF24_PWR_HIGH 2  //  -6 dBm
#define NIUS_NRF24_PWR_MAX  3  //   0 dBm

/* =======================================================================
 * NiusNRF24L01 class
 * ====================================================================== */
class NiusNRF24L01 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /** Hardware SPI on the default bus: cePin=chip enable, csnPin=chip select */
    NiusNRF24L01(uint8_t cePin, uint8_t csnPin);

    /**
     * Hardware SPI on an explicit bus. Use this for a second radio on a board
     * with more than one SPI peripheral (for example SPI1 on RP2040/RP2350).
     * The caller owns bus pin routing; this driver only drives CE and CSN.
     */
    NiusNRF24L01(uint8_t cePin, uint8_t csnPin, SPIClass &spi);

    /** Software SPI */
    NiusNRF24L01(uint8_t cePin, uint8_t csnPin,
                 uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin);

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the NRF24L01.
     * Returns true if the chip responds, false otherwise.
     */
    bool begin();

    /** isReady() — Returns true if the module is initialised. */
    bool isReady();

    /** reset() — Flush TX/RX FIFOs and clear status flags. */
    void reset();

    /** getVersion() — Returns "NRF24L01" or "NRF24L01+" (auto-detected). */
    String getVersion();

    /* -------------------------------------------------------------------
     * Configuration
     * ------------------------------------------------------------------ */

    /**
     * setChannel() — Set the RF channel (frequency).
     * channel — 0 to 125 (2400 + channel MHz)
     * Returns true on success.
     */
    bool setChannel(uint8_t channel);

    /**
     * setDataRate() — Set the air data rate.
     * rate — NIUS_NRF24_250KBPS, NIUS_NRF24_1MBPS, or NIUS_NRF24_2MBPS
     * Returns true on success.
     */
    bool setDataRate(uint8_t rate);

    /**
     * setPower() — Set the transmit power level.
     * level — NIUS_NRF24_PWR_MIN to NIUS_NRF24_PWR_MAX
     * Returns true on success.
     */
    bool setPower(uint8_t level);

    /**
     * setAddress() — Set the local address (used for the primary RX pipe).
     * addr — pointer to the address bytes
     * len  — address length in bytes (2 to 5, default 5)
     * Returns true on success.
     */
    bool setAddress(uint8_t *addr, uint8_t len);

    /**
     * setAutoAck() — Enable or disable Enhanced ShockBurst auto-acknowledge on
     * every pipe. Both ends of a link must agree. Default: enabled.
     */
    bool setAutoAck(bool enabled);

    /**
     * setRetries() — Auto-retransmit timing and count.
     * delay — 0..15, wait = (delay + 1) * 250 us
     * count — 0..15 retransmits (0 disables retransmission)
     */
    bool setRetries(uint8_t delay, uint8_t count);

    /** isPlus() — True when NRF24L01+ silicon (250 kbps capable) was detected. */
    bool isPlus() const { return _plus; }

    /** hasDynamicPayload() — True when dynamic payload length is active. */
    bool hasDynamicPayload() const { return _dynamicPayload; }

    /* -------------------------------------------------------------------
     * Pipe management
     * ------------------------------------------------------------------ */

    /**
     * openWritingPipe() — Set the destination address for TX operations.
     * addr — 5-byte address
     */
    void openWritingPipe(uint8_t *addr);

    /**
     * openReadingPipe() — Open an RX pipe with a specific address.
     * pipe — pipe number 0–5
     * addr — 5-byte address
     */
    void openReadingPipe(uint8_t pipe, uint8_t *addr);

    /* -------------------------------------------------------------------
     * Data transfer
     * ------------------------------------------------------------------ */

    /** startListening() — Switch to RX mode. */
    void startListening();

    /** stopListening() — Switch to standby/TX mode. */
    void stopListening();

    /**
     * available() — Returns true if data is available in the RX FIFO.
     */
    bool available();

    /**
     * readRadio() — Read a payload from the RX FIFO.
     * buf — buffer to receive the payload
     * len — number of bytes to read
     * Returns true on success.
     */
    bool readRadio(uint8_t *buf, uint8_t len);

    /**
     * writeRadio() — Transmit a payload.
     * buf — data to send
     * len — number of bytes (1–32)
     * Returns true if the packet was acknowledged.
     */
    bool writeRadio(uint8_t *buf, uint8_t len);

    /* -------------------------------------------------------------------
     * Diagnostics
     * ------------------------------------------------------------------ */

    /** getStatus() — Return the NRF24 STATUS register byte. */
    uint8_t getStatus();

    /** testCarrier() — Returns true if a carrier is detected on the channel. */
    bool testCarrier();

    /**
     * lastSendFailed() — True when the most recent writeRadio() ended in
     * MAX_RT (retransmits exhausted) rather than a timeout or success.
     */
    bool lastSendFailed() const { return _maxRetryFail; }

private:
    uint8_t   _cePin;
    uint8_t   _csnPin;
    uint8_t   _sckPin;
    uint8_t   _mosiPin;
    uint8_t   _misoPin;
    bool      _softSPI;
    bool      _ready;
    SPIClass *_spi;            // hardware bus, null while in software-SPI mode
    bool      _plus;           // NRF24L01+ silicon detected
    bool      _dynamicPayload; // dynamic payload length active
    bool      _listening;      // currently in PRIM_RX
    bool      _maxRetryFail;   // last writeRadio() hit MAX_RT
    uint8_t   _addrWidth;      // 3..5
    uint8_t   _payloadSize;    // static width used when DPL is unavailable

    uint8_t readReg(uint8_t reg);
    void    writeReg(uint8_t reg, uint8_t val);
    void    readRegBuf(uint8_t reg, uint8_t *buf, uint8_t len);
    void    writeRegBuf(uint8_t reg, const uint8_t *buf, uint8_t len);
    uint8_t sendCommand(uint8_t cmd);
    void    beginTransfer();
    void    endTransfer();
    void    ceHigh();
    void    ceLow();
    uint8_t spiTransfer(uint8_t data);
    uint8_t softTransfer(uint8_t data);
};

#endif // NIUS_NRF24L01_H
