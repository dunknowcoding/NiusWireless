/*
 * NiusBase.h — Abstract base class for all NiusWireless modules
 *
 * Every module driver inherits from this class and implements the four
 * pure-virtual methods.  Higher-level module-specific APIs are added in
 * each subclass.
 *
 * Supported architectures: AVR, SAMD, ESP32, ESP8266, NRF52 (ArduinoNRF),
 *   STM32, RP2040 (Pico/Pico W), Renesas RA (UNO R4 WiFi), and any other
 *   board that provides the Arduino core headers.
 */

#ifndef NIUS_BASE_H
#define NIUS_BASE_H

#include <Arduino.h>

/* -----------------------------------------------------------------------
 * Console serial — SAMD native-USB boards (Zero / M0-Mini) expose the
 * monitor on SerialUSB; Serial is wired to the EDBG UART and is absent on
 * RobotDyn M0-Mini clones. Other boards keep using Serial.
 * ---------------------------------------------------------------------- */
#if defined(ARDUINO_ARCH_SAMD) && defined(USBCON)
  #define NIUS_SERIAL SerialUSB
#else
  #define NIUS_SERIAL Serial
#endif

/* -----------------------------------------------------------------------
 * Return-code constants shared by all NiusWireless modules
 * ---------------------------------------------------------------------- */

#define NIUS_OK             (uint8_t)0x00  // Success
#define NIUS_ERR_NOTAG      (uint8_t)0x01  // No card / device in range
#define NIUS_ERR_TIMEOUT    (uint8_t)0x02  // Operation timed out
#define NIUS_ERR_CRC        (uint8_t)0x03  // CRC mismatch
#define NIUS_ERR_COLLISION  (uint8_t)0x04  // Bit collision detected
#define NIUS_ERR_AUTH       (uint8_t)0x05  // Authentication failed
#define NIUS_ERR_OVERFLOW   (uint8_t)0x06  // Buffer overflow
#define NIUS_ERR_PARAM      (uint8_t)0x07  // Bad parameter
#define NIUS_ERR_UNKNOWN    (uint8_t)0xFF  // Unclassified error

/* -----------------------------------------------------------------------
 * Board-capability detection macros
 * These help module drivers pick the right Serial / SPI / I2C instance.
 * ---------------------------------------------------------------------- */

// Renesas RA (Arduino UNO R4 family)
#if defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RENESAS_UNO) \
 || defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_UNO_R4_MINIMA)
  #define NIUS_BOARD_RENESAS
  #define NIUS_BOARD_NAME "Arduino UNO R4"

// ArduinoNRF nRF52 boards (all variants from ArduinoNRF package)
#elif defined(ARDUINO_ARCH_NRF52) || defined(NRF52_SERIES) \
   || defined(ARDUINO_NRF52_PROMICRO) || defined(ARDUINO_NRF52_NICENANO_V2) \
   || defined(ARDUINO_NRF52_SUPERMINI)  || defined(ARDUINO_NRF52_MINI) \
   || defined(ARDUINO_NRF52_XIAO)       || defined(ARDUINO_NRF52_DEVBOARD) \
   || defined(ARDUINO_NRF52_DEVBOARD_833) || defined(ARDUINO_NRF52_NRFMICRO) \
   || defined(ARDUINO_NRF52_PITAYA_GO)  || defined(ARDUINO_NRF52_USB_DONGLE)
  #define NIUS_BOARD_NRF52
  #define NIUS_BOARD_NAME "nRF52"

// Raspberry Pi Pico / Pico W (RP2040 / RP2350)
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
  #define NIUS_BOARD_RP2040
  #define NIUS_BOARD_NAME "RP2040/RP2350"

// ESP32 family
#elif defined(ARDUINO_ARCH_ESP32)
  #define NIUS_BOARD_ESP32
  #define NIUS_BOARD_NAME "ESP32"

// ESP8266
#elif defined(ARDUINO_ARCH_ESP8266)
  #define NIUS_BOARD_ESP8266
  #define NIUS_BOARD_NAME "ESP8266"

// STM32 (official Arduino STM32 core or stm32duino)
#elif defined(ARDUINO_ARCH_STM32) || defined(STM32F1xx) || defined(STM32F4xx) \
   || defined(STM32L4xx) || defined(STM32H7xx)
  #define NIUS_BOARD_STM32
  #define NIUS_BOARD_NAME "STM32"

// SAMD (Zero, MKR, etc.)
#elif defined(ARDUINO_ARCH_SAMD)
  #define NIUS_BOARD_SAMD
  #define NIUS_BOARD_NAME "SAMD"

// Classic AVR (UNO, Mega, Nano, etc.)
#elif defined(ARDUINO_ARCH_AVR)
  #define NIUS_BOARD_AVR
  #define NIUS_BOARD_NAME "AVR"

#else
  #define NIUS_BOARD_UNKNOWN
  #define NIUS_BOARD_NAME "Unknown"
#endif

/* -----------------------------------------------------------------------
 * NiusBase — abstract base class
 * ---------------------------------------------------------------------- */

class NiusBase {
public:
    virtual ~NiusBase() {}

    /**
     * begin() — Initialise the module hardware.
     * Call once from setup().
     * Returns true on success, false if the module is not detected.
     */
    virtual bool begin() = 0;

    /**
     * isReady() — Returns true if the module is initialised and responding.
     */
    virtual bool isReady() = 0;

    /**
     * reset() — Perform a software reset of the module.
     * The module remains initialised after this call.
     */
    virtual void reset() = 0;

    /**
     * getVersion() — Returns a human-readable firmware / hardware version
     * string, e.g. "MFRC522 v2.0" or "HC-12 V2.6".
     */
    virtual String getVersion() = 0;
};

#endif // NIUS_BASE_H
