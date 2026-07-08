/*
 * NiusRC522.h — RFID-RC522 (MFRC522) module driver
 *
 * Supports both hardware SPI and software (bit-bang) SPI.
 * The RC522 module uses SPI Mode 0 (CPOL=0, CPHA=0), MSB first.
 *
 * --- Constructors ---
 *
 *   // Hardware SPI (SCK/MOSI/MISO on board's default SPI pins)
 *   NiusRC522 rfid(csPin, rstPin);
 *
 *   // Software SPI (any GPIO pins; required for the reference wiring below)
 *   NiusRC522 rfid(csPin, rstPin, sckPin, mosiPin, misoPin);
 *
 * --- Reference wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  → A4 / SDA  (chip-select)
 *   RC522 SCK  → A5 / SCL  (software SPI clock)
 *   RC522 MOSI → D11
 *   RC522 MISO → D12
 *   RC522 IRQ  → D13       (optional interrupt, see setIRQPin)
 *   RC522 RST  → D10
 *   RC522 3.3V → 3.3V
 *   RC522 GND  → GND
 *
 * --- Supported boards ---
 *   AVR (UNO, Mega, Nano), SAMD (Zero, MKR), ESP32, ESP8266,
 *   NRF52 (ArduinoNRF package), STM32, RP2040 (Pico/Pico W),
 *   Renesas RA (Arduino UNO R4 WiFi / Minima)
 */

#ifndef NIUS_RC522_H
#define NIUS_RC522_H

#include <Arduino.h>
#include <SPI.h>
#include "../../NiusBase.h"
#include "NiusMFRC522_Reg.h"

/* -----------------------------------------------------------------------
 * Card-type identifiers returned by getCardType()
 * ---------------------------------------------------------------------- */
#define NIUS_CARD_UNKNOWN      0x00
#define NIUS_CARD_MIFARE_MINI  0x01  // 320 B, 5 sectors
#define NIUS_CARD_MIFARE_1K    0x02  // 1 KB,  16 sectors (S50)
#define NIUS_CARD_MIFARE_4K    0x03  // 4 KB,  40 sectors
#define NIUS_CARD_MIFARE_UL    0x04  // MIFARE Ultralight
#define NIUS_CARD_MIFARE_PLUS  0x05  // MIFARE Plus
#define NIUS_CARD_ISO14443_4   0x06  // ISO 14443-4 compliant
#define NIUS_CARD_ISO18092     0x07  // ISO 18092 / NFC-IP1
#define NIUS_CARD_TNP3XXX      0x08  // NXP TNP3xxx
#define NIUS_CARD_DESFIRE      0x09  // MIFARE DESFire

/* -----------------------------------------------------------------------
 * Antenna-gain constants for setAntennaGain()
 * ---------------------------------------------------------------------- */
#define NIUS_GAIN_18DB    MFRC522_GAIN_18DB
#define NIUS_GAIN_23DB    MFRC522_GAIN_23DB
#define NIUS_GAIN_33DB    MFRC522_GAIN_33DB
#define NIUS_GAIN_38DB    MFRC522_GAIN_38DB
#define NIUS_GAIN_43DB    MFRC522_GAIN_43DB
#define NIUS_GAIN_48DB    MFRC522_GAIN_48DB

/* -----------------------------------------------------------------------
 * Key-type constants for authenticate()
 * ---------------------------------------------------------------------- */
#define NIUS_KEY_A   MIFARE_AUTH_KEY_A   // 0x60
#define NIUS_KEY_B   MIFARE_AUTH_KEY_B   // 0x61

/* -----------------------------------------------------------------------
 * Default MIFARE Classic factory key (all 0xFF)
 * ---------------------------------------------------------------------- */
#define NIUS_KEY_DEFAULT_LEN  6
extern const uint8_t NIUS_KEY_DEFAULT[NIUS_KEY_DEFAULT_LEN];

/* -----------------------------------------------------------------------
 * Maximum UID length (10 bytes for triple-size UID)
 * ---------------------------------------------------------------------- */
#define NIUS_UID_MAX_LEN  10

/* =======================================================================
 * NiusRC522 class
 * ====================================================================== */
class NiusRC522 : public NiusBase {
public:

    /* -------------------------------------------------------------------
     * Constructors
     * ------------------------------------------------------------------ */

    /**
     * Hardware SPI constructor.
     *
     * The board's default SPI bus (SPI.h) is used.
     * csPin   — chip-select (SS) pin
     * rstPin  — reset pin
     */
    NiusRC522(uint8_t csPin, uint8_t rstPin);

    /**
     * Software SPI constructor.
     *
     * Any GPIO pins can be used for SCK, MOSI and MISO.
     * Use this constructor when the RC522 SCK is not on the board's
     * hardware SCK pin (e.g. the reference wiring uses SCL / A5).
     *
     * csPin   — chip-select pin        (RC522 SDA)
     * rstPin  — reset pin              (RC522 RST)
     * sckPin  — SPI clock pin          (RC522 SCK)
     * mosiPin — master-out/slave-in    (RC522 MOSI)
     * misoPin — master-in/slave-out    (RC522 MISO)
     */
    NiusRC522(uint8_t csPin, uint8_t rstPin,
              uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin);

    /* -------------------------------------------------------------------
     * NiusBase interface
     * ------------------------------------------------------------------ */

    /**
     * begin() — Initialise the RC522.
     * Configures GPIO, resets the chip and verifies communication.
     * Returns true if the chip responds, false otherwise.
     */
    bool begin();

    /**
     * begin(spiSpeed) — Initialise with a specific SPI clock frequency.
     * Only affects hardware SPI mode; ignored in software SPI mode.
     * spiSpeed — SPI clock in Hz (default 4 000 000, max 10 000 000)
     */
    bool begin(uint32_t spiSpeed);

    /**
     * isReady() — Returns true if the RC522 is initialised and responding.
     */
    bool isReady();

    /**
     * reset() — Perform a soft reset (MFRC522 SoftReset command).
     * The chip stays operational after the call.
     */
    void reset();

    /**
     * getVersion() — Returns a version string such as "MFRC522 v2.0".
     * Returns "MFRC522 unknown" if the chip does not respond.
     */
    String getVersion();

    /* -------------------------------------------------------------------
     * Card detection
     * ------------------------------------------------------------------ */

    /**
     * cardPresent() — Scan for a card in the antenna field.
     *
     * When this function returns true, the UID, uidLen and lastCardType
     * fields are populated and the card is in the ACTIVE state.
     * Call halt() after processing to return the card to IDLE/HALT state.
     *
     * Returns true if a card was found and selected, false otherwise.
     *
     * Note: uses REQA, so a card in HALT state will not be detected until
     * it is physically removed from the field and returned. Use
     * cardPresentWake() if you need to detect HALT'd cards.
     */
    bool cardPresent();

    /**
     * cardPresentWake() — Like cardPresent() but also wakes HALT'd cards
     * (uses WUPA instead of REQA).
     */
    bool cardPresentWake();

    /**
     * getUIDBytes() — Copy the UID of the last detected card into buf.
     * buf  — byte array with at least NIUS_UID_MAX_LEN bytes of space
     * len  — set to the number of valid UID bytes (4, 7 or 10)
     * Returns true if a UID is available (a card was detected).
     */
    bool getUIDBytes(uint8_t *buf, uint8_t &len);

    /**
     * getUID() — Return the UID of the last detected card as a hex String.
     * Example: "A1B2C3D4" for a 4-byte UID.
     * Returns an empty String if no card has been detected.
     */
    String getUID();

    /**
     * getCardType() — Return the card-type constant of the last detected
     * card (one of the NIUS_CARD_* defines above).
     */
    uint8_t getCardType();

    /**
     * getCardTypeName() — Return a human-readable card-type string.
     * Example: "MIFARE Classic 1K"
     */
    String getCardTypeName();

    /**
     * halt() — Send the ISO 14443 HALT command to the active card.
     * Call this after you have finished processing a card.
     * The card will not respond to REQA until it leaves the field.
     */
    void halt();

    /* -------------------------------------------------------------------
     * MIFARE Classic operations
     * Authenticate before calling readBlock() / writeBlock().
     * ------------------------------------------------------------------ */

    /**
     * authenticate() — Perform MIFARE Classic authentication.
     *
     * blockAddr — block number to authenticate (0–63 for 1K, 0–255 for 4K)
     * keyType   — NIUS_KEY_A or NIUS_KEY_B
     * key       — pointer to a 6-byte key array
     *
     * Returns NIUS_OK on success, NIUS_ERR_AUTH on failure.
     *
     * Example (using default factory key):
     *   uint8_t key[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
     *   rfid.authenticate(4, NIUS_KEY_A, key);
     */
    uint8_t authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key);

    /**
     * readBlock() — Read 16 bytes from a MIFARE Classic block.
     *
     * blockAddr — block number (must be authenticated first)
     * data      — pointer to a 16-byte buffer that receives the data
     *
     * Returns NIUS_OK on success.
     */
    uint8_t readBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * writeBlock() — Write 16 bytes to a MIFARE Classic block.
     *
     * blockAddr — block number (must be authenticated first)
     * data      — pointer to a 16-byte buffer to write
     *
     * Returns NIUS_OK on success.
     *
     * WARNING: Writing block 0 (manufacturer block) or sector trailers
     * incorrectly can permanently lock the card. Double-check the data
     * and block address before calling this function.
     */
    uint8_t writeBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * stopCrypto() — End the MIFARE authentication session.
     * Call this after you have finished all read/write operations on a card
     * so that the crypto engine is released before the next card scan.
     */
    void stopCrypto();

    /* -------------------------------------------------------------------
     * RF antenna control
     * ------------------------------------------------------------------ */

    /** antennaOn()  — Turn the RF antenna on. */
    void antennaOn();

    /** antennaOff() — Turn the RF antenna off. */
    void antennaOff();

    /**
     * setAntennaGain() — Set the receiver gain.
     * gain — one of the NIUS_GAIN_* constants (default NIUS_GAIN_48DB)
     */
    void setAntennaGain(uint8_t gain);

    /**
     * getAntennaGain() — Return the current gain setting (NIUS_GAIN_* value).
     */
    uint8_t getAntennaGain();

    /* -------------------------------------------------------------------
     * IRQ (interrupt) pin
     * ------------------------------------------------------------------ */

    /**
     * setIRQPin() — Attach an interrupt pin for card-detected events.
     * irqPin — GPIO pin connected to RC522 IRQ (active LOW)
     * After calling this, you can use attachInterrupt() on the same pin
     * in your sketch to get notified asynchronously.
     */
    void setIRQPin(uint8_t irqPin);

    /* -------------------------------------------------------------------
     * Advanced / raw register access
     * Use these for features not covered by the high-level API.
     * ------------------------------------------------------------------ */

    /** readRegister()  — Read one MFRC522 register byte. */
    uint8_t readRegister(uint8_t addr);

    /** writeRegister() — Write one byte to an MFRC522 register. */
    void writeRegister(uint8_t addr, uint8_t value);

    /** setRegisterBits()   — Set (OR) specific bits in a register. */
    void setRegisterBits(uint8_t addr, uint8_t mask);

    /** clearRegisterBits() — Clear (AND NOT) specific bits in a register. */
    void clearRegisterBits(uint8_t addr, uint8_t mask);

    /* -------------------------------------------------------------------
     * Public state (populated by cardPresent / cardPresentWake)
     * ------------------------------------------------------------------ */

    uint8_t uid[NIUS_UID_MAX_LEN]; // Raw UID bytes of the last detected card
    uint8_t uidLen;                // Number of valid bytes in uid[]
    uint8_t lastCardType;          // NIUS_CARD_* of the last detected card
    uint8_t lastError;             // Error code from the last cardPresent call
    uint8_t lastSelectError;       // Error from selectCard() — 0 if not reached

private:

    /* --- Pin configuration -------------------------------------------- */
    uint8_t  _csPin;
    uint8_t  _rstPin;
    uint8_t  _sckPin;
    uint8_t  _mosiPin;
    uint8_t  _misoPin;
    uint8_t  _irqPin;
    bool     _softSPI;
    uint32_t _spiSpeed;
    bool     _ready;

    /* --- Low-level SPI helpers ---------------------------------------- */
    void    csLow();
    void    csHigh();
    uint8_t spiTransfer(uint8_t data);
    uint8_t softTransfer(uint8_t data);

    /* --- Multi-byte register I/O -------------------------------------- */
    void readRegisterBurst(uint8_t addr, uint8_t count,
                           uint8_t *values, uint8_t rxAlign);
    void writeRegisterBurst(uint8_t addr, uint8_t count, uint8_t *values);

    /* --- MFRC522 internal operations ---------------------------------- */
    uint8_t executeCommand(uint8_t cmd,
                           uint8_t waitIRQ,
                           uint8_t *sendData, uint8_t sendLen,
                           uint8_t *backData, uint8_t *backLen,
                           uint8_t *validBits,
                           uint8_t rxAlign,
                           bool checkCRC);
    uint8_t calcCRC(uint8_t *data, uint8_t len, uint8_t *result);
    bool    verifyCRC(uint8_t *data, uint8_t len);

    /* --- ISO 14443A card protocol ------------------------------------- */
    uint8_t requestA(uint8_t *atqa);
    uint8_t wakeupA(uint8_t *atqa);
    uint8_t requestOrWakeup(uint8_t cmd, uint8_t *atqa);
    uint8_t selectCard(uint8_t *outUID, uint8_t *outLen, uint8_t *outSAK);

    /* --- Card-type helper --------------------------------------------- */
    uint8_t sakToCardType(uint8_t sak);
};

#endif // NIUS_RC522_H
