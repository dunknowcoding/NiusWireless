/*
 * NiusLoRaBase.h — Abstract base class for all NiusWireless LoRa/radio drivers
 *
 * Both SX127x (RFM95/96/98, RA-01/02) and SX126x (SX1262, SX1268) drivers
 * inherit from this class.  It extends NiusBase with LoRa-specific operations
 * that are common across all supported chips.
 *
 * Bandwidth / SF / CR notation used throughout this library:
 *   Bandwidth — actual Hz value:  125000, 250000, 500000 (or 7800 … 500000)
 *   Spreading factor — integer:   6 … 12
 *   Coding rate — denominator:    5 … 8  (i.e. 4/5, 4/6, 4/7, 4/8)
 *   TX power — signed dBm:        -9 … +22  (chip dependent)
 */

#ifndef NIUS_LORA_BASE_H
#define NIUS_LORA_BASE_H

#include <Arduino.h>
#include <SPI.h>
#include "../../NiusBase.h"

/* -----------------------------------------------------------------------
 * LoRa return / status codes (extend NIUS_* codes)
 * ---------------------------------------------------------------------- */
#define NIUS_LORA_OK              NIUS_OK           // 0x00
#define NIUS_LORA_ERR_BUSY        (uint8_t)0x10     // Chip BUSY
#define NIUS_LORA_ERR_TIMEOUT     NIUS_ERR_TIMEOUT  // 0x02
#define NIUS_LORA_ERR_CRC         NIUS_ERR_CRC      // 0x03
#define NIUS_LORA_ERR_PARAM       NIUS_ERR_PARAM    // 0x07
#define NIUS_LORA_ERR_CHIP        (uint8_t)0x11     // Chip not detected

/* -----------------------------------------------------------------------
 * Frequency constants (convenience, pass your own float for other bands)
 * ---------------------------------------------------------------------- */
#define NIUS_LORA_FREQ_433        433.0f   // MHz
#define NIUS_LORA_FREQ_470        470.0f
#define NIUS_LORA_FREQ_868        868.0f
#define NIUS_LORA_FREQ_915        915.0f

/* -----------------------------------------------------------------------
 * Default LoRa parameters
 * ---------------------------------------------------------------------- */
#define NIUS_LORA_DEFAULT_BW      125000UL // 125 kHz
#define NIUS_LORA_DEFAULT_SF      7
#define NIUS_LORA_DEFAULT_CR      5        // 4/5
#define NIUS_LORA_DEFAULT_PREAMBLE 8
#define NIUS_LORA_DEFAULT_POWER   17       // dBm

/* =======================================================================
 * NiusLoRaBase — abstract LoRa base class
 * ====================================================================== */
class NiusLoRaBase : public NiusBase {
public:
    virtual ~NiusLoRaBase() {}

    /* -------------------------------------------------------------------
     * NiusBase interface (must be implemented by chip-specific subclass)
     * ------------------------------------------------------------------ */
    virtual bool   begin()          = 0;
    virtual bool   isReady()        = 0;
    virtual void   reset()          = 0;
    virtual String getVersion()     = 0;

    /* -------------------------------------------------------------------
     * Frequency and modulation
     * ------------------------------------------------------------------ */

    /**
     * setFrequency() — Set the RF centre frequency.
     * freq — frequency in MHz, e.g. 915.0 or 433.175
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setFrequency(float freqMHz) = 0;

    /**
     * setBandwidth() — Set the LoRa signal bandwidth.
     * bwHz — bandwidth in Hz.  Common values:
     *          7800, 10400, 15600, 20800, 31250, 41700, 62500,
     *          125000 (default), 250000, 500000
     * Returns NIUS_LORA_OK on success.
     * Note: bandwidths below 62 500 Hz are not available on SX1276 HF bands.
     */
    virtual uint8_t setBandwidth(uint32_t bwHz) = 0;

    /**
     * setSpreadingFactor() — Set the LoRa spreading factor.
     * sf — 6 to 12.  Higher values increase range but reduce data rate.
     *       SF6 requires implicit header mode.
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setSpreadingFactor(uint8_t sf) = 0;

    /**
     * setCodingRate() — Set the LoRa forward-error-correction coding rate.
     * cr — denominator of 4/x: 5 (4/5, default), 6 (4/6), 7 (4/7), 8 (4/8)
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setCodingRate(uint8_t cr) = 0;

    /**
     * setTxPower() — Set the transmit output power.
     * dBm — power in dBm.  Range is chip/PA dependent (typically -9 to +22).
     *        PA_BOOST is automatically selected when dBm > 14 on SX127x.
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setTxPower(int8_t dBm) = 0;

    /**
     * setPreambleLength() — Set the number of preamble symbols.
     * length — 6 to 65535 (default 8).
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setPreambleLength(uint16_t length) = 0;

    /**
     * setSyncWord() — Set the LoRa sync word.
     * syncWord — 8-bit value:
     *   SX127x: 0x12 = private network (default), 0x34 = LoRaWAN public
     *   SX126x: 0x1424 = private, 0x3444 = LoRaWAN (two bytes stored internally)
     *   Pass the low byte for SX126x; this function handles the conversion.
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t setSyncWord(uint8_t syncWord) = 0;

    /**
     * enableCRC() / disableCRC() — Enable or disable payload CRC.
     * CRC is enabled by default.
     */
    virtual uint8_t enableCRC()  = 0;
    virtual uint8_t disableCRC() = 0;

    /**
     * setExplicitHeader() — Use LoRa explicit header mode (default).
     * The packet header carries length, CR and CRC info automatically.
     */
    virtual uint8_t setExplicitHeader() = 0;

    /**
     * setImplicitHeader() — Use LoRa implicit header mode.
     * payloadLen — fixed payload length in bytes (required in implicit mode)
     * Use this with SF6, or when minimising packet overhead.
     */
    virtual uint8_t setImplicitHeader(uint8_t payloadLen) = 0;

    /* -------------------------------------------------------------------
     * Packet TX
     * ------------------------------------------------------------------ */

    /**
     * beginPacket() — Start building a TX packet.
     * Call write() / writeBuf() one or more times, then endPacket().
     * Returns NIUS_LORA_OK if ready to accept data.
     */
    virtual uint8_t beginPacket() = 0;

    /**
     * write() — Append one byte to the TX packet buffer.
     * Returns the number of bytes written (1 on success, 0 if full).
     */
    virtual uint8_t write(uint8_t byte) = 0;

    /**
     * writeBuf() — Append multiple bytes to the TX packet buffer.
     * data — pointer to the data bytes
     * len  — number of bytes to write
     * Returns the number of bytes actually written.
     */
    virtual uint8_t writeBuf(uint8_t *data, uint8_t len) = 0;

    /**
     * writeStr() — Append a null-terminated C-string to the TX packet.
     * str — the string to send (without the null terminator)
     * Returns the number of bytes written.
     */
    uint8_t writeStr(const char *str) {
        uint8_t n = 0;
        while (*str) {
            if (write((uint8_t)*str++) == 0) break;
            n++;
        }
        return n;
    }

    /**
     * endPacket() — Finish the packet and transmit it.
     * async — if true, return immediately; if false (default), block until TX done.
     * Returns NIUS_LORA_OK on success (or when transmission starts if async).
     */
    virtual uint8_t endPacket(bool async = false) = 0;

    /**
     * isTxDone() — Poll TX completion in async mode.
     * Returns true when the packet has been transmitted.
     */
    virtual bool isTxDone() = 0;

    /* -------------------------------------------------------------------
     * Packet RX
     * ------------------------------------------------------------------ */

    /**
     * startReceive() — Put the radio into continuous receive mode.
     * In this mode the radio listens indefinitely.
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t startReceive() = 0;

    /**
     * startReceiveSingle() — Put the radio into single receive mode.
     * The radio exits RX after one packet is received (or on timeout).
     * Returns NIUS_LORA_OK on success.
     */
    virtual uint8_t startReceiveSingle() = 0;

    /**
     * parsePacket() — Check if a complete LoRa packet has been received.
     * Returns the number of payload bytes available (0 if no packet).
     * Call readByte() / readBuf() after this to get the data.
     */
    virtual uint8_t parsePacket() = 0;

    /**
     * available() — Number of unread bytes in the last received packet.
     */
    virtual uint8_t available() = 0;

    /**
     * readByte() — Read one byte from the received packet.
     * Returns -1 if no data is available.
     */
    virtual int readByte() = 0;

    /**
     * readBuf() — Read multiple bytes from the received packet.
     * buf  — output buffer (caller must ensure it is large enough)
     * len  — maximum bytes to read
     * Returns the number of bytes actually read.
     */
    virtual uint8_t readBuf(uint8_t *buf, uint8_t len) = 0;

    /* -------------------------------------------------------------------
     * Signal quality (valid after parsePacket() returns > 0)
     * ------------------------------------------------------------------ */

    /** getRSSI() — Packet RSSI in dBm (typically -30 to -130). */
    virtual int16_t getRSSI() = 0;

    /** getLastRSSI() — Ambient RSSI (last sampled, not from a packet). */
    virtual int16_t getLastRSSI() = 0;

    /** getSNR() — Packet SNR in dB (typically -20 to +10). */
    virtual float getSNR() = 0;

    /* -------------------------------------------------------------------
     * Power management
     * ------------------------------------------------------------------ */

    /** sleep() — Put the radio into sleep (lowest power) mode. */
    virtual uint8_t sleep() = 0;

    /** standby() — Put the radio into standby mode. */
    virtual uint8_t standby() = 0;

    /* -------------------------------------------------------------------
     * Channel Activity Detection (CAD)
     * ------------------------------------------------------------------ */

    /**
     * isChannelActive() — Perform CAD and return true if a LoRa signal
     * is detected on the current channel.
     * This function blocks until CAD completes (~symbol periods).
     */
    virtual bool isChannelActive() = 0;

    /* -------------------------------------------------------------------
     * Interrupt / callback support
     * ------------------------------------------------------------------ */

    /**
     * setDIO0Callback() — Attach a function to be called when DIO0 fires.
     * Use attachInterrupt(digitalPinToInterrupt(dio0Pin), cb, RISING) in
     * your sketch; this function registers the handler inside the driver.
     */
    void setTxDoneCallback(void (*cb)())  { _onTxDone = cb; }
    void setRxDoneCallback(void (*cb)())  { _onRxDone = cb; }

    /* -------------------------------------------------------------------
     * Public state
     * ------------------------------------------------------------------ */

    uint8_t  lastError;      // Last operation error code
    uint8_t  lastRxLen;      // Bytes in the last received packet
    int16_t  lastRssi;       // RSSI of last received packet (dBm)
    float    lastSnr;        // SNR  of last received packet (dB)

protected:
    void (*_onTxDone)();
    void (*_onRxDone)();

    NiusLoRaBase() :
        lastError(NIUS_OK), lastRxLen(0), lastRssi(0), lastSnr(0.0f),
        _onTxDone(nullptr), _onRxDone(nullptr) {}
};

#endif // NIUS_LORA_BASE_H
