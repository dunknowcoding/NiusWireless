/*
 * NiusLoRaSX127x.h — SX1276 / SX1277 / SX1278 LoRa driver
 *
 * Covers: RFM95W (868/915 MHz), RFM96W (433 MHz), RFM98W (915 MHz),
 *         RA-01 (433 MHz SX1278), RA-02 (433 MHz SX1278 + SMA)
 *
 * Convenience typedefs at the bottom let you write:
 *   NiusRFM95  radio(csPin, rstPin, dio0Pin);   // or NiusRFM96, etc.
 *
 * --- Pinout reference (Adafruit/common breakout silkscreen) ---
 *   Pin label   Function
 *   GND         Ground
 *   Vin/VCC     3.3 V (DO NOT connect 5 V directly to the module)
 *   SCK         SPI clock
 *   MISO        SPI MISO
 *   MOSI        SPI MOSI
 *   CS / NSS    SPI chip-select (active LOW)
 *   RST / RESET Reset (active LOW)
 *   G0 / DIO0   Primary IRQ (TxDone / RxDone / CadDone)
 *   G1 / DIO1   RxTimeout / FhssChangeChannel
 *   G2 / DIO2   FhssChangeChannel / FSK
 *   G3-G5       Additional GPIO (unused by this driver)
 *   ANT         Antenna (ALWAYS attach an antenna before transmitting!)
 *
 * --- RA-01 / RA-02 silkscreen (Ai-Thinker, common AliExpress version) --
 *   GND, VCC, SCK, MISO, MOSI, NSS, DIO0, DIO1, DIO2, DIO3, DIO4, DIO5, RESET
 *
 * --- SPI Mode ---
 *   Mode 0 (CPOL=0, CPHA=0), MSB first, maximum 10 MHz.
 *
 * --- Supported boards ---
 *   AVR (UNO, Mega), SAMD, ESP32, NRF52 (ArduinoNRF), STM32, RP2040,
 *   Renesas RA (Arduino UNO R4 WiFi / Minima)
 */

#ifndef NIUS_LORA_SX127X_H
#define NIUS_LORA_SX127X_H

#include <Arduino.h>
#include <SPI.h>
#include "NiusLoRaBase.h"
#include "NiusLoRa_Regs.h"

/* Maximum payload bytes the SX127x FIFO can hold */
#define SX127X_MAX_PAYLOAD  255

/* Default SPI speed (can be overridden with begin(freq, spiSpeed)) */
#define SX127X_DEFAULT_SPI_SPEED  8000000UL

/* =======================================================================
 * NiusLoRaSX127x — SX1276 / SX1277 / SX1278 driver
 * ====================================================================== */
class NiusLoRaSX127x : public NiusLoRaBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * Hardware SPI constructor.
     *
     * csPin   — SPI chip-select (NSS) pin
     * rstPin  — hardware reset pin
     * dio0Pin — DIO0 interrupt pin (TxDone / RxDone)
     *
     * Example (Adafruit RFM95W breakout, UNO R4 WiFi):
     *   NiusLoRaSX127x radio(10, 9, 2);
     */
    NiusLoRaSX127x(uint8_t csPin, uint8_t rstPin, uint8_t dio0Pin);

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the SX127x at 915.0 MHz with default parameters
     * (BW 125 kHz, SF7, CR 4/5, power 17 dBm, explicit header, CRC on).
     * Returns true if the chip responds, false otherwise.
     */
    bool begin();

    /**
     * begin(freqMHz) — Initialise at the given frequency.
     * freqMHz — centre frequency in MHz (e.g. 433.0, 868.0, 915.0).
     * Returns true on success.
     */
    bool begin(float freqMHz);

    /**
     * begin(freqMHz, spiSpeed) — Initialise with a specific SPI clock rate.
     * spiSpeed — SPI clock in Hz (default 8 000 000, max 10 000 000).
     * Returns true on success.
     */
    bool begin(float freqMHz, uint32_t spiSpeed);

    /** isReady() — Returns true if the chip is initialised. */
    bool isReady();

    /** reset() — Pull RST low then release; waits for chip to restart. */
    void reset();

    /** getVersion() — Returns e.g. "SX127x v0x12". */
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
    int32_t getFrequencyError();

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
     * LNA / OCP (advanced)
     * ------------------------------------------------------------------ */

    /**
     * setLNAGain() — Override automatic LNA gain control.
     * gain — 1 (maximum) to 6 (minimum).  Use 0 to restore AGC.
     */
    uint8_t setLNAGain(uint8_t gain);

    /**
     * setOCP() — Set over-current protection threshold.
     * mA — current in mA.  Common values: 45 … 240 mA.
     */
    uint8_t setOCP(uint8_t mA);

    /* -------------------------------------------------------------------
     * Raw register access
     * ------------------------------------------------------------------ */

    /** readRegister()  — Read one SX127x register byte. */
    uint8_t readRegister(uint8_t addr);

    /** writeRegister() — Write one byte to an SX127x register. */
    void writeRegister(uint8_t addr, uint8_t value);

    /** setRegisterBits()   — Set (OR) specific bits in a register. */
    void setRegisterBits(uint8_t addr, uint8_t mask);

    /** clearRegisterBits() — Clear (AND NOT) specific bits in a register. */
    void clearRegisterBits(uint8_t addr, uint8_t mask);

    /* -------------------------------------------------------------------
     * DIO0 ISR helper
     * Call this from your attachInterrupt handler:
     *   attachInterrupt(digitalPinToInterrupt(DIO0_PIN), onDIO0, RISING);
     *   void onDIO0() { radio.handleDIO0(); }
     * ------------------------------------------------------------------ */
    void handleDIO0();

private:
    uint8_t  _csPin;
    uint8_t  _rstPin;
    uint8_t  _dio0Pin;
    uint32_t _spiSpeed;
    bool     _ready;
    float    _freqMHz;
    uint8_t  _txBufLen;
    uint8_t  _rxBufPtr;
    uint8_t  _rxBufLen;
    bool     _implicitHeader;
    bool     _txAsync;

    void     csLow();
    void     csHigh();
    uint8_t  spiTransfer(uint8_t data);

    /* Set operating mode (sleep, standby, tx, rx…) */
    void setOpMode(uint8_t mode);

    /* Convert band-specific parameters */
    uint8_t  bwToBits(uint32_t bwHz, bool &isLowFreq);
};

/* -----------------------------------------------------------------------
 * Convenience typedefs — use whichever name matches your module label
 * ---------------------------------------------------------------------- */
typedef NiusLoRaSX127x NiusRFM95;   // RFM95W  (SX1276, 868/915 MHz)
typedef NiusLoRaSX127x NiusRFM96;   // RFM96W  (SX1278,   433 MHz)
typedef NiusLoRaSX127x NiusRFM98;   // RFM98W  (SX1276,   915 MHz)
typedef NiusLoRaSX127x NiusRA01;    // RA-01   (SX1278,   433 MHz)
typedef NiusLoRaSX127x NiusRA02;    // RA-02   (SX1278,   433 MHz + SMA)

#endif // NIUS_LORA_SX127X_H
