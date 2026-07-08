/*
 * NiusLoRaSX126x.h — SX1261 / SX1262 / SX1268 LoRa driver
 *
 * Covers all SX126x family modules:
 *   SX1261 — up to +15 dBm (PA_LP pin)
 *   SX1262 — up to +22 dBm (PA_HP pin), 868/915 MHz
 *   SX1268 — up to +22 dBm (PA_HP pin), 433/470 MHz
 *
 * --- Key differences from SX127x ---
 *   1. Command-based SPI protocol (not register map)
 *   2. BUSY pin — must poll low before every command
 *   3. Only DIO1 exposed for IRQ (no DIO0-DIO5 style)
 *   4. Different register addresses and modulation params
 *   5. TXCO support (DIO3 can drive crystal oscillator)
 *
 * --- Typical e-commerce module pinout (EBYTE E22, Ai-Thinker Ra-06, etc.) ---
 *   Pin label   Function
 *   GND         Ground
 *   VCC         3.3 V
 *   SCK         SPI clock
 *   MOSI        SPI MOSI
 *   MISO        SPI MISO
 *   NSS / CS    SPI chip-select (active LOW)
 *   NRESET/RST  Reset (active LOW)
 *   BUSY        Busy indicator (HIGH = chip processing, do not send commands)
 *   DIO1        Interrupt output (TxDone / RxDone / Timeout / etc.)
 *   ANT         Antenna  (ALWAYS attach before transmitting!)
 *
 * --- SPI Mode ---
 *   Mode 0 (CPOL=0, CPHA=0), MSB first, maximum 16 MHz.
 *
 * --- Supported boards ---
 *   AVR (UNO, Mega), SAMD, ESP32, NRF52 (ArduinoNRF), STM32, RP2040,
 *   Renesas RA (Arduino UNO R4 WiFi / Minima)
 */

#ifndef NIUS_LORA_SX126X_H
#define NIUS_LORA_SX126X_H

#include <Arduino.h>
#include <SPI.h>
#include "NiusLoRaBase.h"
#include "NiusLoRa_Regs.h"

#define SX126X_MAX_PAYLOAD    255
#define SX126X_DEFAULT_SPI_SPEED  8000000UL

/* =======================================================================
 * NiusLoRaSX126x — SX1261 / SX1262 / SX1268 driver
 * ====================================================================== */
class NiusLoRaSX126x : public NiusLoRaBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * Hardware SPI constructor.
     *
     * csPin    — SPI chip-select (NSS) pin
     * rstPin   — hardware reset pin
     * busyPin  — BUSY pin (chip signals it is processing)
     * dio1Pin  — DIO1 interrupt pin (TxDone / RxDone / Timeout)
     *
     * Example:
     *   NiusLoRaSX126x radio(10, 9, 8, 2);  // CS, RST, BUSY, DIO1
     */
    NiusLoRaSX126x(uint8_t csPin, uint8_t rstPin,
                   uint8_t busyPin, uint8_t dio1Pin);

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise at 915.0 MHz with default parameters.
     * Returns true if the chip responds, false otherwise.
     */
    bool begin();

    /** begin(freqMHz) — Initialise at the given frequency. */
    bool begin(float freqMHz);

    /** begin(freqMHz, spiSpeed) — Initialise with a specific SPI clock. */
    bool begin(float freqMHz, uint32_t spiSpeed);

    bool   isReady();
    void   reset();
    String getVersion();

    /* -------------------------------------------------------------------
     * Frequency and modulation
     * ------------------------------------------------------------------ */

    uint8_t setFrequency(float freqMHz);
    uint8_t setBandwidth(uint32_t bwHz);
    uint8_t setSpreadingFactor(uint8_t sf);
    uint8_t setCodingRate(uint8_t cr);
    uint8_t setTxPower(int8_t dBm);
    uint8_t setPreambleLength(uint16_t length);
    uint8_t setSyncWord(uint8_t syncWord);
    uint8_t enableCRC();
    uint8_t disableCRC();
    uint8_t setExplicitHeader();
    uint8_t setImplicitHeader(uint8_t payloadLen);

    /* -------------------------------------------------------------------
     * Packet TX
     * ------------------------------------------------------------------ */

    uint8_t beginPacket();
    uint8_t write(uint8_t byte);
    uint8_t writeBuf(uint8_t *data, uint8_t len);
    uint8_t endPacket(bool async = false);
    bool    isTxDone();

    /* -------------------------------------------------------------------
     * Packet RX
     * ------------------------------------------------------------------ */

    uint8_t startReceive();
    uint8_t startReceiveSingle();
    uint8_t parsePacket();
    uint8_t available();
    int     readByte();
    uint8_t readBuf(uint8_t *buf, uint8_t len);

    /* -------------------------------------------------------------------
     * Signal quality
     * ------------------------------------------------------------------ */

    int16_t getRSSI();
    int16_t getLastRSSI();
    float   getSNR();

    /* -------------------------------------------------------------------
     * Power management
     * ------------------------------------------------------------------ */

    uint8_t sleep();
    uint8_t standby();

    /* -------------------------------------------------------------------
     * CAD
     * ------------------------------------------------------------------ */

    bool isChannelActive();

    /* -------------------------------------------------------------------
     * Advanced
     * ------------------------------------------------------------------ */

    /**
     * setRegulatorMode() — Select the internal power regulator.
     * useDCDC — true = DC-DC (lower current draw), false = LDO (default).
     * Only effective on boards that have an external inductor for DC-DC.
     */
    uint8_t setRegulatorMode(bool useDCDC);

    /**
     * setDIO2AsRFSwitch() — Route DIO2 to control an external RF switch.
     * Some modules (e.g. EBYTE E22) need this for TX/RX antenna switching.
     */
    uint8_t setDIO2AsRFSwitch(bool enable);

    /**
     * setTCXO() — Enable TCXO control on DIO3.
     * voltage — TCXO reference voltage (1.6 V … 3.3 V).
     *           Use 0 to disable.
     * delayMs — startup delay in ms.
     */
    uint8_t setTCXO(float voltage, uint32_t delayMs);

    /* -------------------------------------------------------------------
     * Raw command / register access
     * ------------------------------------------------------------------ */

    /**
     * sendCommand() — Send a raw SX126x command.
     * cmd    — command byte (SX126X_CMD_*)
     * params — parameter bytes to follow (may be nullptr)
     * nParams — number of parameter bytes
     */
    void sendCommand(uint8_t cmd, uint8_t *params, uint8_t nParams);

    /**
     * sendCommandReadResponse() — Send a command and read back response bytes.
     * response — output buffer
     * nResponse — number of bytes to read
     */
    void sendCommandReadResponse(uint8_t cmd, uint8_t *params, uint8_t nParams,
                                 uint8_t *response, uint8_t nResponse);

    /** writeReg() — Write bytes to an SX126x register address (16-bit addr). */
    void writeReg(uint16_t addr, uint8_t *data, uint8_t len);

    /** readReg() — Read bytes from an SX126x register address. */
    void readReg(uint16_t addr, uint8_t *data, uint8_t len);

    /* -------------------------------------------------------------------
     * DIO1 ISR helper
     * ------------------------------------------------------------------ */
    void handleDIO1();

private:
    uint8_t  _csPin;
    uint8_t  _rstPin;
    uint8_t  _busyPin;
    uint8_t  _dio1Pin;
    uint32_t _spiSpeed;
    bool     _ready;
    float    _freqMHz;

    uint8_t  _txBuf[SX126X_MAX_PAYLOAD];
    uint8_t  _txBufLen;
    uint8_t  _rxBuf[SX126X_MAX_PAYLOAD];
    uint8_t  _rxBufLen;
    uint8_t  _rxBufPtr;

    bool     _implicitHeader;
    uint8_t  _payloadLen;
    uint8_t  _bwCode;
    uint8_t  _sfCode;
    uint8_t  _crCode;
    bool     _crcOn;
    uint16_t _preambleLen;

    void    waitBusy();
    void    csLow();
    void    csHigh();
    uint8_t spiTransfer(uint8_t data);

    void    applyModulationParams();
    void    applyPacketParams();
    uint8_t bwToCode(uint32_t bwHz);
};

/* -----------------------------------------------------------------------
 * Convenience typedefs
 * ---------------------------------------------------------------------- */
typedef NiusLoRaSX126x NiusSX1261;
typedef NiusLoRaSX126x NiusSX1262;
typedef NiusLoRaSX126x NiusSX1268;

#endif // NIUS_LORA_SX126X_H
