/*
 * NiusRC522.h — RFID-RC522 (MFRC522) module driver
 *
 * Supports hardware SPI, software (bit-bang) SPI, and I2C transports.
 * - SPI mode uses Mode 0 (CPOL=0, CPHA=0), MSB first.
 * - I2C mode uses the chip's I2C bus. MFRC522 default address is 0x28;
 *   some boards pull I2C_ADD0 HIGH for 0x29.
 *
 * --- Constructors ---
 *
 *   // Hardware SPI (SCK/MOSI/MISO on board's default SPI pins)
 *   NiusRC522 rfid(csPin, rstPin);
 *
 *   // Software SPI (any GPIO pins; required for the reference wiring below)
 *   NiusRC522 rfid(csPin, rstPin, sckPin, mosiPin, misoPin);
 *
 *   // I2C (uses board's default Wire bus)
 *   NiusRC522 rfid(Wire, 0x28, rstPin);  // Wire ref + 7-bit I2C address + RST
 *
 * --- Reference wiring (Arduino UNO R4 WiFi) ---
 *   RC522 SDA  → A4 / SDA  (chip-select in SPI mode)
 *   RC522 SCK  → A5 / SCL  (software SPI clock)
 *   RC522 MOSI → D11
 *   RC522 MISO → D12
 *   RC522 IRQ  → D13       (optional interrupt, see setIRQPin)
 *   RC522 RST  → D10
 *   RC522 3.3V → 3.3V
 *   RC522 GND  → GND
 *
 *   For I2C, the chip's I2C_SDA / I2C_SCL pins go to the board's Wire SDA/SCL.
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
#include <Wire.h>
#include "../../NiusBase.h"
#include "NiusMFRC522_Reg.h"

/* Card-type identifiers: see NiusBase.h (NIUS_CARD_*). */

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

    /**
     * I2C constructor.
     *
     * Uses the MFRC522 chip's I2C bus via the supplied TwoWire reference
     * (typically `Wire`). The board's SDA / SCL pins must be wired to
     * the chip's I2C_SDA / I2C_SCL pins.
     *
     * The MFRC522 datasheet's default I2C address is 0x28 (I2C_ADD0 LOW)
     * or 0x29 (I2C_ADD0 HIGH, depending on the board wiring). Pass the
     * 7-bit address — Wire handles the R/W bit.
     *
     * `rstPin` is the chip's hardware reset pin (RC522 RST).
     */
    NiusRC522(TwoWire &bus, uint8_t i2cAddress, uint8_t rstPin);

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
     * getATQA() — Return the ATQA bytes from the last cardPresent() call
     * as a hex String. Example: "0400" for a MIFARE Classic 1K.
     * Returns an empty String if no card has been detected.
     */
    String getATQA();

    /**
     * getATQABytes() — Copy the two ATQA bytes from the last cardPresent()
     * call into buf. Returns true if a card has been detected.
     */
    bool getATQABytes(uint8_t *buf);

    /**
     * getSAK() — Return the SAK (Select Acknowledge) byte from the last
     * cardPresent() call. Returns 0 if no card has been detected.
     */
    uint8_t getSAK();

    /**
     * getNTAGVersion() — Send GET_VERSION (0x60) to a MIFARE Ultralight
     * or NTAG tag and copy the 8-byte version response into `version`.
     * Returns NIUS_OK on success, NIUS_ERR_TIMEOUT / NIUS_ERR_UNKNOWN
     * if the tag is not Ultralight-family or does not respond.
     *
     * The response bytes are:
     *   [0] fixed header (0x00)
     *   [1] vendor ID  (0x04 for NXP)
     *   [2] product type (0x03 = Ultralight, 0x04 = NTAG)
     *   [3] product subtype
     *   [4] major version
     *   [5] minor version
     *   [6] storage size
     *   [7] protocol type
     */
    uint8_t getNTAGVersion(uint8_t *version);

    /**
     * halt() — Send the ISO 14443 HALT command to the active card.
     * Call this after you have finished processing a card.
     * The card will not respond to REQA until it leaves the field.
     */
    void halt();

    /* -------------------------------------------------------------------
     * Convenience
     * ------------------------------------------------------------------ */

    /**
     * printInfo() — Print UID / ATQA / SAK / type-name of the
     * last detected card to `out` (default: Serial).
     * One-call replacement for printing each getter individually.
     */
    void printInfo(Print &out = Serial);

    /**
     * dumpToSerial() — Type-adaptive memory dump. Reads the entire
     * user-accessible memory of the last detected card and prints it
     * block-by-block (or 4-page groups for Ultralight / NTAG) to the
     * Serial Monitor.
     *
     *   MIFARE Classic 1K / 4K / Mini  -> dumpClassic() with `key`
     *   MIFARE Ultralight / NTAG      -> dumpUltralight()
     *   Other types                    -> "not supported for this family"
     *
     * For Classic, `key` may be `nullptr` to use NIUS_KEY_DEFAULT.
     *
     * Returns the number of sectors (Classic) or pages (Ultralight) read.
     */
    uint8_t dumpToSerial(const uint8_t *key = nullptr);

    /* -------------------------------------------------------------------
     * Debug / diagnostics
     * Use these when a card isn't responding and you need to know
     * whether the firmware or the card is at fault.
     * ------------------------------------------------------------------ */

    /**
     * printRegisters() — Print the MFRC522 chip's configuration
     * registers to `out` (default: Serial). The output groups them as:
     *   - chip identification (VersionReg)
     *   - protocol state (CommandReg, ModeReg, TxASKReg)
     *   - timer (TMode / TPrescaler / TReload)
     *   - RF antenna state (TxControlReg)
     *
     * If these registers don't look right, the chip is misconfigured
     * (firmware problem). If they look fine and the card still doesn't
     * respond, the card is the problem.
     */
    void printRegisters(Print &out = Serial);

    /**
     * printStatus() — Print the post-transceive debug registers to
     * `out` (default: Serial) and decode any set error-flag bits:
     *   - ComIrqReg (0x04)  - interrupt source
     *   - ErrorReg  (0x06)  - protocol / buffer / parity errors
     *   - Status2Reg (0x08) - Crypto1 / FIFO state
     *   - FIFOLevelReg (0x0A)
     *
     * Call after a failed transceive to see *why* it failed.
     */
    void printStatus(Print &out = Serial);

    /**
     * powerCycle() — Antenna off for `holdMs` milliseconds, then
     * antenna on and a fresh `reset()`. Used when a stuck card's
     * protocol state has latched; the field-down period lets the
     * card-side cap discharge so it boots from a fresh state.
     *
     * `holdMs` default = 1000 ms. On long power-downs the MFRC522
     * analog state can drift on counterfeit chips (VersionReg=0x18);
     * the embedded `reset()` covers that.
     */
    void powerCycle(uint16_t holdMs = 1000);

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
     * blockAddr — block number (must be authenticated first). The
     *             function refuses blockAddr past the card's actual
     *             memory (63 for 1K/Mini, 255 for 4K) so you don't
     *             accidentally read into OTP / lock / reserved regions.
     * data      — pointer to a 16-byte buffer that receives the data
     *
     * Returns NIUS_OK on success, NIUS_ERR_PARAM on out-of-range address
     * or non-Classic card.
     */
    uint8_t readBlock(uint8_t blockAddr, uint8_t *data);

    /**
     * writeBlock() — Write 16 bytes to a MIFARE Classic block.
     *
     * blockAddr — block number (must be authenticated first)
     * data      — pointer to a 16-byte buffer to write
     * force     — false (default) refuses writes to block 0 and to
     *             sector trailers (block 3, 7, 11, ...), because those
     *             can permanently lock the card if written incorrectly.
     *             Pass force=true only when you know what you're doing
     *             (e.g. via setUid() which manages block 0 safely).
     *
     * Returns NIUS_OK on success, NIUS_ERR_PARAM on bounds or
     * sensitive-block violation.
     *
     * WARNING: With force=true, writing block 0 or a sector trailer
     * incorrectly can permanently lock the card.
     */
    uint8_t writeBlock(uint8_t blockAddr, uint8_t *data, bool force = false);

    /**
     * stopCrypto() — End the MIFARE authentication session.
     * Call this after you have finished all read/write operations on a card
     * so that the crypto engine is released before the next card scan.
     */
    void stopCrypto();

    /**
     * setUid() — Change the UID of a MIFARE Classic card. Equivalent to
     * the standard MFRC522 library's `MIFARE_SetUid(newUid, uidSize, true)`
     * but with a safety pass that the original lacks.
     *
     *   - Dry-run by default (commit = false). The first call prints
     *     "old UID / new UID / old BCC / new BCC / manufacturer bytes
     *     preserved" and returns without writing anything. To actually
     *     perform the write, call again with commit = true.
     *   - Computes BCC = XOR of all UID bytes (the original library had
     *     a bug here that bricked several tags).
     *   - Reads block 0 first and preserves bytes uidSize+1..15 (mfr +
     *     padding). The original library zeroed these out, which bricks
     *     some clones.
     *   - Authenticates sector 0 with the factory key (Key A, falling
     *     back to Key B).
     *   - After writing, halts + re-detects and compares the new UID
     *     against the requested one; returns NIUS_ERR_UNKNOWN if the
     *     card reports a different UID (which means the write was
     *     rejected or partially completed).
     *
     * newUid  — pointer to the new UID bytes (4 bytes for 1K / Mini,
     *           7 bytes for 4K)
     * uidSize — number of UID bytes (must be 4 or 7)
     * commit  — false (default) = preview only; true = actually write
     *
     * Returns NIUS_OK on success, or one of NIUS_ERR_* otherwise.
     */
    uint8_t setUid(uint8_t *newUid, uint8_t uidSize, bool commit = false);

    /**
     * dumpClassic() — Dump every MIFARE Classic block that the supplied
     * key can authenticate. Equivalent to the standard library's
     * `PICC_DumpToSerial(&(uid))` but factored to take a callback
     * function so you can route the output to Serial, an LCD, a file, etc.
     *
     * key         — 6-byte key to try on every sector (use
     *                `{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}` for the factory key)
     * printer     — function pointer that receives one 16-byte block at a
     *                time. Pass `nullptr` if you only want the summary.
     *
     * Returns the number of sectors that authenticated successfully.
     */
    uint8_t dumpClassic(uint8_t *key, void (*printer)(uint8_t *));

    /**
     * dumpUltralight() — Dump every MIFARE Ultralight / NTAG page that
     * responds. Each call to `printer` passes 16 bytes (4 consecutive
     * pages). Stops on the first NAK. Returns the number of pages dumped.
     *
     * printer — function pointer that receives one 16-byte chunk at a
     *           time. Pass `nullptr` if you only want the count.
     */
    uint8_t dumpUltralight(void (*printer)(uint8_t *));

    /* -------------------------------------------------------------------
     * Low-level transceive
     * Use these for custom protocols, raw command access, or to implement
     * key-recovery attacks (nested authentication, darkside, etc.).
     * ------------------------------------------------------------------- */

    /**
     * transceive() — Send a raw command to the card and receive the
     * response. This is the lowest-level data-path: it builds a
     * Transceive command on the MFRC522, waits for the FIFO, copies
     * the response back. Equivalent to the standard MFRC522 library's
     * PCD_TransceiveData().
     *
     * sendData   — bytes to send (without CRC; the chip appends it)
     * sendLen    — number of bytes to send (0..16)
     * backData   — buffer for the response (must be ≥ sendLen + 2)
     * backLen    — in: capacity of backData; out: number of bytes received
     * validBits  — out: number of valid bits in the last received byte
     *               (used for short ACKs like 4-bit NAK); pass nullptr
     *               if you don't need it
     * rxAlign    — bit position to align the first received byte (0 = none)
     * checkCRC   — if true (default), the MFRC522 hardware verifies the
     *               CRC in the response; if it fails, the function returns
     *               NIUS_ERR_CRC
     *
     * Returns NIUS_OK on success, an NIUS_ERR_* code otherwise.
     */
    uint8_t transceive(uint8_t *sendData, uint8_t sendLen,
                       uint8_t *backData, uint8_t *backLen,
                       uint8_t *validBits = nullptr,
                       uint8_t rxAlign = 0,
                       bool checkCRC = true);

    /* -------------------------------------------------------------------
     * MIFARE Ultralight / NTAG operations
     * No authentication required. Pages are 4 bytes, addressed by page
     * number. READ returns 4 consecutive pages (16 bytes) at a time.
     * Suitable for MIFARE Ultralight, Ultralight C, Ultralight EV1,
     * NTAG213/215/216, and "magic" CUID cards in their post-write mode.
     * ------------------------------------------------------------------ */

    /**
     * readPage() — Read 4 consecutive pages (16 bytes) starting at `page`
     * from a MIFARE Ultralight / NTAG tag. No authentication required.
     *
     * page — page number (0 for first page)
     * data — pointer to a 16-byte buffer that receives the data
     *
     * Returns NIUS_OK on success.
     */
    uint8_t readPage(uint8_t page, uint8_t *data);

    /**
     * writePage() — Write 4 bytes to a single page on a MIFARE Ultralight /
     * NTAG tag. No authentication required. The card must NOT be
     * password-protected, or you must have authenticated with the PWD_AUTH
     * command first.
     *
     * page — page number
     * data — pointer to a 4-byte buffer
     *
     * Returns NIUS_OK on success, NIUS_ERR_AUTH on NAK.
     */
    uint8_t writePage(uint8_t page, uint8_t *data);

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
    uint8_t atqa[2];               // ATQA bytes from the last REQA/WUPA
    uint8_t sak;                   // SAK byte from the last SELECT (with bit2 masked off)
    uint8_t lastCardType;          // NIUS_CARD_* of the last detected card
    uint8_t lastError;             // Error code from the last cardPresent call
    uint8_t lastSelectError;       // Error from selectCard() — 0 if not reached

private:

    /* --- MFRC522 internal operations ----------------------------------- */
    uint8_t executeCommand(uint8_t cmd,
                           uint8_t waitIRQ,
                           uint8_t *sendData, uint8_t sendLen,
                           uint8_t *backData, uint8_t *backLen,
                           uint8_t *validBits,
                           uint8_t rxAlign,
                           bool checkCRC);
    uint8_t calcCRC(uint8_t *data, uint8_t len, uint8_t *result);
    bool    verifyCRC(uint8_t *data, uint8_t len);

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

    /* --- I2C transport ------------------------------------------------ */
    bool     _useI2C;       // true when constructed with the I2C ctor
    TwoWire *_i2c;          // Wire (or Wire1 / Wire2) the chip is on
    uint8_t  _i2cAddress;    // 7-bit address

    /* --- Low-level SPI helpers ---------------------------------------- */
    void    csLow();
    void    csHigh();
    uint8_t spiTransfer(uint8_t data);
    uint8_t softTransfer(uint8_t data);

    /* --- Multi-byte register I/O -------------------------------------- */
    void readRegisterBurst(uint8_t addr, uint8_t count,
                           uint8_t *values, uint8_t rxAlign);
    void writeRegisterBurst(uint8_t addr, uint8_t count, uint8_t *values);
    uint8_t requestA(uint8_t *atqa);
    uint8_t wakeupA(uint8_t *atqa);
    uint8_t requestOrWakeup(uint8_t cmd, uint8_t *atqa);
    uint8_t selectCard(uint8_t *outUID, uint8_t *outLen, uint8_t *outSAK);

    /* --- Card-type helper --------------------------------------------- */
    uint8_t sakToCardType(uint8_t sak);
};

#endif // NIUS_RC522_H
