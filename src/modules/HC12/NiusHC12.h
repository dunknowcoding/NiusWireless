/*
 * NiusHC12.h — HC-12 long-range serial wireless module driver
 *
 * The HC-12 appears as a transparent UART bridge to the MCU.
 * Configuration is done via AT commands by pulling the SET pin LOW.
 * Default baud rate: 9600 bps, channel 1 (433.4 MHz), FU3 mode, power 20 dBm.
 *
 * Pinout:
 *   VCC  — 3.2 V – 5.5 V
 *   GND  — Ground
 *   TXD  — UART transmit (connect to MCU RX)
 *   RXD  — UART receive  (connect to MCU TX)
 *   SET  — AT command mode (pull LOW to enter config)
 *
 * --- Status: STUB — full implementation coming in a future release ---
 */

#ifndef NIUS_HC12_H
#define NIUS_HC12_H

#include <Arduino.h>
#include "../../NiusBase.h"

/* -----------------------------------------------------------------------
 * HC-12 operating modes for setMode()
 * ---------------------------------------------------------------------- */
#define NIUS_HC12_FU1  1  // Short distance, low power, 250 kbps baud on air
#define NIUS_HC12_FU2  2  // Medium distance, medium power
#define NIUS_HC12_FU3  3  // Long distance, full power (default)
#define NIUS_HC12_FU4  4  // Ultra-long distance, 1200 bps air baud

/* =======================================================================
 * NiusHC12 class
 * ====================================================================== */
class NiusHC12 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * Hardware UART constructor.
     * serial   — reference to a HardwareSerial object (e.g. Serial1)
     * setPin   — GPIO pin connected to HC-12 SET (used for AT mode)
     * baudRate — initial UART baud rate (default 9600)
     */
    NiusHC12(HardwareSerial &serial, uint8_t setPin, uint32_t baudRate = 9600);

#if defined(ARDUINO_ARCH_AVR)
    /**
     * Software UART constructor (AVR only — uses SoftwareSerial).
     * rxPin    — receive pin
     * txPin    — transmit pin
     * setPin   — AT mode control pin
     * baudRate — baud rate (SoftwareSerial supports up to 115200 on some boards)
     */
    NiusHC12(uint8_t rxPin, uint8_t txPin, uint8_t setPin, uint32_t baudRate = 9600);
#endif

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the HC-12.
     * Configures SET pin, starts the serial port and verifies AT response.
     * Returns true if the module replies to "AT", false otherwise.
     */
    bool begin();

    /** isReady() — Returns true if the module is initialised. */
    bool isReady();

    /**
     * reset() — Reset the HC-12 to factory defaults (AT+DEFAULT command).
     * The baud rate reverts to 9600 after this call.
     */
    void reset();

    /** getVersion() — Returns the HC-12 firmware version string. */
    String getVersion();

    /* -------------------------------------------------------------------
     * Transparent data transfer
     * (SET pin must be HIGH — normal operating mode)
     * ------------------------------------------------------------------ */

    /** send() — Transmit a String over the air. */
    void send(String data);

    /** sendBytes() — Transmit raw bytes over the air. */
    void sendBytes(uint8_t *data, uint8_t len);

    /** available() — Returns the number of received bytes waiting. */
    int available();

    /**
     * receive() — Return all waiting bytes as a String.
     * Blocks up to timeoutMs milliseconds for the first byte.
     */
    String receive(uint32_t timeoutMs = 100);

    /** readByte() — Read and return one byte, or -1 if none available. */
    int readByte();

    /* -------------------------------------------------------------------
     * AT configuration commands
     * (The SET pin is driven LOW automatically and restored after the call)
     * ------------------------------------------------------------------ */

    /**
     * setChannel() — Set the radio channel.
     * channel — 1 to 100  (433.4 + (channel-1)*0.4 MHz)
     * Returns true on success.
     */
    bool setChannel(uint8_t channel);

    /**
     * setPower() — Set the transmit power level.
     * level — 1 (−1 dBm) to 8 (20 dBm)
     * Returns true on success.
     */
    bool setPower(uint8_t level);

    /**
     * setBaud() — Set the UART baud rate stored in HC-12.
     * baud — one of: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
     * NOTE: Call begin() again with the new rate after this.
     * Returns true on success.
     */
    bool setBaud(uint32_t baud);

    /**
     * setMode() — Set the operating mode.
     * mode — NIUS_HC12_FU1 … NIUS_HC12_FU4
     * Returns true on success.
     */
    bool setMode(uint8_t mode);

    /**
     * getChannel() — Return the current channel number (1–100).
     * Returns 0 on error.
     */
    uint8_t getChannel();

    /* -------------------------------------------------------------------
     * AT command helpers
     * ------------------------------------------------------------------ */

    /** enterATMode() — Pull SET pin LOW and wait for the module to enter AT mode. */
    bool enterATMode();

    /** exitATMode() — Release SET pin HIGH to return to transparent mode. */
    void exitATMode();

    /**
     * sendAT() — Send a raw AT command string (e.g. "AT+B9600").
     * enterATMode() must be called first.
     */
    void sendAT(String cmd);

    /**
     * readResponse() — Read the module's response to an AT command.
     * timeoutMs — maximum wait time in milliseconds
     */
    String readResponse(uint32_t timeoutMs = 500);

private:
    HardwareSerial *_hwSerial;
    uint8_t         _setPin;
    uint32_t        _baudRate;
    bool            _atMode;
    bool            _ready;
};

#endif // NIUS_HC12_H
