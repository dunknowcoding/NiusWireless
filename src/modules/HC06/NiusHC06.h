/*
 * NiusHC06.h — HC-06 / HC-05 Bluetooth Classic SPP module driver
 *
 * The HC-06 (slave-only) and HC-05 (master/slave) present a transparent
 * UART-to-Bluetooth SPP (Serial Port Profile) bridge.
 * AT commands are used for configuration while no Bluetooth connection
 * is active (baud rate for AT mode is fixed at 38400 on HC-06 v3+).
 *
 * Pinout:
 *   VCC   — 3.6 V – 6 V (onboard 3.3 V regulator)
 *   GND   — Ground
 *   TXD   — UART transmit  (connect to MCU RX)
 *   RXD   — UART receive   (connect to MCU TX, level-shift to 3.3 V)
 *   STATE — HIGH when connected (HC-05 also has EN/KEY pin)
 *
 * --- Status: STUB — full implementation coming in a future release ---
 */

#ifndef NIUS_HC06_H
#define NIUS_HC06_H

#include <Arduino.h>
#include "../../NiusBase.h"

/* =======================================================================
 * NiusHC06 class  (also covers HC-05)
 * ====================================================================== */
class NiusHC06 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * Hardware UART constructor.
     * serial   — HardwareSerial instance (e.g. Serial1)
     * baudRate — operating baud rate (default 9600)
     */
    NiusHC06(HardwareSerial &serial, uint32_t baudRate = 9600);

#if defined(ARDUINO_ARCH_AVR)
    /**
     * Software UART constructor (AVR only).
     * rxPin, txPin — GPIO pins for SoftwareSerial
     * baudRate     — operating baud rate
     */
    NiusHC06(uint8_t rxPin, uint8_t txPin, uint32_t baudRate = 9600);
#endif

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the HC-06.
     * Returns true if the module responds to "AT".
     */
    bool begin();

    /** isReady() — Returns true if the module is initialised. */
    bool isReady();

    /**
     * reset() — Power-cycle equivalent: close and re-open serial port.
     * Does not change stored configuration on the module.
     */
    void reset();

    /** getVersion() — Returns "HC-06" or "HC-05". */
    String getVersion();

    /* -------------------------------------------------------------------
     * Transparent data transfer
     * ------------------------------------------------------------------ */

    /** send() — Send a String over Bluetooth SPP. */
    void send(String data);

    /** sendBytes() — Send raw bytes. */
    void sendBytes(uint8_t *data, uint8_t len);

    /** available() — Returns the number of bytes waiting in the RX buffer. */
    int available();

    /** receive() — Read all waiting bytes into a String (non-blocking). */
    String receive();

    /** readByte() — Read one byte, or -1 if none available. */
    int readByte();

    /* -------------------------------------------------------------------
     * AT configuration commands
     * NOTE: There must be NO active Bluetooth connection while using AT.
     * ------------------------------------------------------------------ */

    /**
     * setName() — Set the Bluetooth device name (1–20 characters).
     * Example: rfid.setName("MySensor")
     * Returns true on success.
     */
    bool setName(String name);

    /**
     * setBaud() — Set the UART baud rate stored in the module.
     * baud — one of: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
     * NOTE: Call begin() again with the new rate after this.
     * Returns true on success.
     */
    bool setBaud(uint32_t baud);

    /**
     * setPIN() — Set the pairing PIN code (4-digit String, e.g. "1234").
     * Returns true on success.
     */
    bool setPIN(String pin);

    /**
     * getName() — Query and return the stored device name.
     * Returns an empty String on error.
     */
    String getName();

    /**
     * getAddress() — Query and return the Bluetooth MAC address.
     * Format: "XXXX:XX:XXXXXX"
     * Returns an empty String on error.
     */
    String getAddress();

    /**
     * isConnected() — Returns true if a Bluetooth device is connected.
     * Requires the STATE pin to be connected and passed to the constructor.
     */
    bool isConnected();

    /* -------------------------------------------------------------------
     * Raw AT helpers
     * ------------------------------------------------------------------ */

    /** sendAT() — Send a raw AT command string (e.g. "AT+NAMEMyDevice"). */
    void sendAT(String cmd);

    /**
     * readResponse() — Read the module response to an AT command.
     * timeoutMs — maximum wait in milliseconds
     */
    String readResponse(uint32_t timeoutMs = 500);

private:
    HardwareSerial *_hwSerial;
    uint32_t        _baudRate;
    bool            _ready;
};

#endif // NIUS_HC06_H
