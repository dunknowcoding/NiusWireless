# NiusWireless API Reference

Version 0.1.0

---

## Table of Contents

1. [Overview](#overview)
2. [Supported Boards](#supported-boards)
3. [Shared (Base) API](#shared-base-api)
4. [NiusRC522 — RFID-RC522 (MFRC522)](#niusrc522--rfid-rc522-mfrc522)
5. [NiusNRF24L01 — NRF24L01+ 2.4 GHz Radio](#niusnrf24l01--nrf24l01-24-ghz-radio)
6. [NiusHC12 — HC-12 Long-Range Serial](#niushc12--hc-12-long-range-serial)
7. [NiusHC06 — HC-06 / HC-05 Bluetooth SPP](#niushc06--hc-06--hc-05-bluetooth-spp)
8. [NiusPN532 — PN532 NFC/RFID](#niuspn532--nfcrfid)
9. [Return Codes](#return-codes)
10. [Card Type Constants](#card-type-constants)
11. [Wiring Quick-Reference](#wiring-quick-reference)

> **Tip** — the RC522 section scales from "I just want to detect a card" → "I want to read/write blocks" → "I want to write raw protocol". If you only need the first level, skip ahead to [Quick start](#rc522-quick-start) and stop reading after [Common helpers](#rc522-common-helpers). For "the card isn't responding — why?", jump to [Debug / diagnostics](#rc522-debug).

---

## Overview

NiusWireless is an expandable Arduino library providing a unified, Arduino-style API
for popular wireless and RF modules.  Include a single header and pick the module
class you need:

```cpp
#include <NiusWireless.h>
```

Each module class inherits four common methods from `NiusBase` (`begin`, `isReady`,
`reset`, `getVersion`) and adds its own module-specific API.

---

## Supported Boards

| Family | Examples | Detection Macro |
|--------|----------|-----------------|
| Renesas RA (UNO R4) | Arduino UNO R4 WiFi, UNO R4 Minima | `ARDUINO_ARCH_RENESAS` |
| ArduinoNRF nRF52 | ProMicro nRF52840, nice!nano v2, SuperMini, XIAO nRF52840 | `ARDUINO_ARCH_NRF52` / `NRF52_SERIES` |
| RP2040 / RP2350 | Raspberry Pi Pico, Pico W, Pico 2 W | `ARDUINO_ARCH_RP2040` |
| ESP32 | ESP32, ESP32-S3, ESP32-C3 | `ARDUINO_ARCH_ESP32` |
| ESP8266 | NodeMCU, Wemos D1 mini | `ARDUINO_ARCH_ESP8266` |
| STM32 | STM32F103 (Blue Pill), STM32F4 | `ARDUINO_ARCH_STM32` |
| SAMD | Arduino Zero, MKR series | `ARDUINO_ARCH_SAMD` |
| AVR | UNO R3, Mega 2560, Nano, Pro Mini | `ARDUINO_ARCH_AVR` |

---

## Shared (Base) API

All module classes implement the following methods, inherited from `NiusBase`.

---

### `begin()`

```cpp
bool begin()
```

Initialise the module hardware.  Configure GPIO pins, start SPI/I2C/UART, reset the
chip, and verify communication.  Call once from `setup()`.

**Returns:** `true` on success, `false` if the module is not detected.

---

### `isReady()`

```cpp
bool isReady()
```

Check whether the module is initialised and responding.

**Returns:** `true` if ready.

---

### `reset()`

```cpp
void reset()
```

Perform a software reset.  The module stays initialised (GPIO and SPI/UART remain
configured) after the call.

---

### `getVersion()`

```cpp
String getVersion()
```

Return a human-readable version string, e.g. `"MFRC522 v2.0"`.

---

## NiusRC522 — RFID-RC522 (MFRC522)

**Header:** `src/modules/RC522/NiusRC522.h`  
**Status:** Full implementation  
**Protocol:** SPI (Mode 0, MSB first, max 10 MHz)

---

### <a id="rc522-quick-start"></a>Quick start

```cpp
#include <NiusWireless.h>

NiusRC522 rfid(SDA, 10, SCL, 11, 12);   // 5 software-SPI pins

void setup() {
    Serial.begin(9600);
    rfid.begin();
}

void loop() {
    if (!rfid.cardPresentWake()) return;   // find + select a tag
    rfid.printInfo();                        // UID / ATQA / SAK / Type
    rfid.dumpToSerial();                     // type-adaptive: classic or UL
    rfid.halt();
}
```

If you only need the UID and a recommended example for the card family, that loop is the whole sketch.

#### Examples shipped with the library

| Sketch | Use it for |
|--------|------------|
| `examples/rc522_spi_basic` | Just detect a tag - UID / ATQA / SAK / type. Closest to a "hello world". |
| `examples/rc522_spi_adv` | One-shot dump + Classic block-0 roundtrip + register inspection. |
| `examples/rc522_spi_s50` | Tightly focused MIFARE Classic 1K write/read/restore demo. |
| `examples/rc522_spi_tag` | Type-adaptive: Classic dump / UL dump / CUID UID-change demo. |
| `examples/rc522_i2c_basic` | Same as `rc522_spi_basic`, but on the chip's I2C bus. |
| `examples/rc522_i2c_adv` | Same as `rc522_spi_adv`, but on the chip's I2C bus; auto-detects Classic vs Ultralight and runs the appropriate operations. |

The six examples above are user-facing. For field debugging - register inspection, post-failure IRQ decoding, bricked-card recovery - use the [`printRegisters` / `printStatus` / `powerCycle` debug methods](#rc522-debug) below; no separate diagnostic sketch is needed.

---

### Constructors

The MFRC522 chip exposes both SPI and I2C. Pick whichever matches your wiring. The `begin()` call detects which constructor was used and initialises the matching bus.

#### Hardware SPI

```cpp
NiusRC522 rfid(csPin, rstPin);
```

Uses the board's default hardware SPI bus.

| Parameter | Type | Description |
|-----------|------|-------------|
| `csPin`   | `uint8_t` | Chip-select (SS) pin |
| `rstPin`  | `uint8_t` | Reset pin |

#### Software SPI

```cpp
NiusRC522 rfid(csPin, rstPin, sckPin, mosiPin, misoPin);
```

Bit-bangs SPI on any GPIO pins.  Required when the RC522 SCK is not on the board's
hardware SCK pin (e.g. the reference wiring uses SCL/A5 as SCK).

| Parameter | Type | Description |
|-----------|------|-------------|
| `csPin` | `uint8_t` | Chip-select (RC522 SDA) |
| `rstPin` | `uint8_t` | Reset (RC522 RST) |
| `sckPin` | `uint8_t` | SPI clock (RC522 SCK) |
| `mosiPin` | `uint8_t` | MOSI (RC522 MOSI) |
| `misoPin` | `uint8_t` | MISO (RC522 MISO) |

**Reference wiring example (Arduino UNO R4 WiFi):**

```cpp
// SDA->D18/A4  SCK->D19/A5  MOSI->D11  MISO->D12  IRQ->D13  RST->D10
NiusRC522 rfid(SDA, 10, SCL, 11, 12);
```

#### I2C

```cpp
NiusRC522 rfid(Wire, 0x28, rstPin);
NiusRC522 rfid(Wire1, 0x29, 10);      // second I2C bus, alternate I2C address
```

Uses the MFRC522 chip's I2C bus via the supplied `TwoWire` reference
(typically `Wire` or `Wire1`). The board's SDA / SCL pins must be wired
to the chip's I2C_SDA / I2C_SCL pins (D18/A4 and D19/A5 on UNO R4 WiFi).

The chip's I2C address is set by the I2C_ADD0 pin:
- `I2C_ADD0 LOW  -> 7-bit address 0x28`
- `I2C_ADD0 HIGH -> 7-bit address 0x29`

Default bus speed is 400 kHz (Fast mode), set by `begin()`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `bus` | `TwoWire &` | The Wire reference (typically `Wire`, or `Wire1`/`Wire2` on boards with multiple buses) |
| `i2cAddress` | `uint8_t` | 7-bit I2C address (`0x28` or `0x29`) |
| `rstPin` | `uint8_t` | Hardware reset pin (RC522 RST) |

**Reference wiring example (Arduino UNO R4 WiFi, address 0x28):**

```cpp
//   chip I2C_SDA -> board SDA (D18/A4)
//   chip I2C_SCL -> board SCL (D19/A5)
//   chip RST     -> D10
#include <Wire.h>
NiusRC522 rfid(Wire, 0x28, 10);
```

> I2C sub-byte alignment (`rxAlign`) is unsupported - the chip itself
> only delivers full bytes over I2C.

---

### `begin()` / `begin(spiSpeed)`

```cpp
bool begin()
bool begin(uint32_t spiSpeed)
```

Initialise the RC522 transport (SPI or I2C, depending on which constructor
was used) and the chip.  The optional `spiSpeed` (default 4 000 000 Hz) only
affects hardware SPI mode; software SPI runs as fast as the MCU GPIO allows.
On I2C, the `spiSpeed` argument is ignored.

**Returns:** `true` if the chip is detected.

---

### `cardPresent()`

```cpp
bool cardPresent()
```

Scan for a card using the ISO 14443A REQA command.  Only detects cards that are NOT
in HALT state.  On success, populates `uid[]`, `uidLen`, and `lastCardType`.

**Returns:** `true` if a card was found and selected.

**Note:** Call `halt()` after processing so the card enters HALT state and is not
re-read on the next call.

---

### `cardPresentWake()`

```cpp
bool cardPresentWake()
```

Like `cardPresent()` but uses WUPA, which also wakes cards in HALT state.

---

### `getUID()`

```cpp
String getUID()
```

Return the UID of the last detected card as an upper-case hex string.

**Returns:** e.g. `"A1B2C3D4"` for a 4-byte UID, or `""` if no card was detected.

---

### `getUIDBytes()`

```cpp
bool getUIDBytes(uint8_t *buf, uint8_t &len)
```

Copy the raw UID bytes of the last detected card into `buf`.

| Parameter | Description |
|-----------|-------------|
| `buf` | Output buffer, at least `NIUS_UID_MAX_LEN` (10) bytes |
| `len` | Set to the number of valid UID bytes (4, 7, or 10) |

**Returns:** `true` if a UID is available.

---

### `getCardType()` / `getCardTypeName()`

```cpp
uint8_t getCardType()
String  getCardTypeName()
```

Return the card type of the last detected card.  `getCardType()` returns one of the
`NIUS_CARD_*` constants.  `getCardTypeName()` returns a human-readable String.

| Constant | Value | Name |
|----------|-------|------|
| `NIUS_CARD_UNKNOWN` | 0x00 | Unknown |
| `NIUS_CARD_MIFARE_MINI` | 0x01 | MIFARE Mini |
| `NIUS_CARD_MIFARE_1K` | 0x02 | MIFARE Classic 1K |
| `NIUS_CARD_MIFARE_4K` | 0x03 | MIFARE Classic 4K |
| `NIUS_CARD_MIFARE_UL` | 0x04 | MIFARE Ultralight (incl. NTAG) |
| `NIUS_CARD_MIFARE_PLUS` | 0x05 | MIFARE Plus |
| `NIUS_CARD_ISO14443_4` | 0x06 | ISO 14443-4 (DESFire etc.) |
| `NIUS_CARD_ISO18092` | 0x07 | ISO 18092 (NFC-IP1) |

To distinguish NTAG from generic MIFARE Ultralight, call `getNTAGVersion()`
— NTAG and Ultralight share the same ATQA/SAK (`0x4400` / `0x00`) but the
`GET_VERSION (0x60)` response carries the product type / subtype.

---

### `getATQA()` / `getATQABytes()` / `getSAK()`

```cpp
String  getATQA()
bool    getATQABytes(uint8_t *buf)
uint8_t getSAK()
```

Return the ATQA (2 bytes) and SAK (1 byte) captured during the last
`cardPresent()` / `cardPresentWake()` call.  These identify the card
family at the protocol level, even before any application-level auth.

`getATQA()` formats the bytes as a 4-hex-digit String (e.g. `"0400"` for
MIFARE Classic 1K, `"4400"` for Ultralight / NTAG). `getATQABytes()`
copies the two raw bytes into `buf` in the order returned by the card
(ATQA[0] = MSB, ATQA[1] = LSB).  `getSAK()` returns the raw SAK byte,
with the CRC bits (`0x80` / `0x20`) still set.

```cpp
if (rfid.cardPresent()) {
    uint8_t atqa[2], sak = rfid.getSAK();
    rfid.getATQABytes(atqa);
    // atqa[0..1] = ATQA, sak = SAK
}
```

---

### `getNTAGVersion()`

```cpp
uint8_t getNTAGVersion(uint8_t *version)
```

Send `GET_VERSION (0x60)` to the tag and copy the 8-byte version response
into `version`.  Returns `NIUS_OK` on success.

| Byte | Meaning | Example (NTAG215) |
|------|---------|-------------------|
| 0 | Fixed header | `0x00` |
| 1 | Vendor ID | `0x04` = NXP |
| 2 | Product type | `0x04` = NTAG, `0x03` = MIFARE Ultralight |
| 3 | Product subtype | `0x11` = NTAG215, `0x01` = Ultralight EV1 |
| 4 | Major version | `0x01` |
| 5 | Minor version | `0x00` |
| 6 | Storage size | `0x12` = 504 bytes (NTAG215) |
| 7 | Protocol type | `0x03` = ISO 14443-3 |

This is the only way to tell apart NTAG213/215/216, MIFARE Ultralight,
and MIFARE Ultralight EV1 from each other — they all share the same
ATQA (`0x4400`) and SAK (`0x00`).

---

### `halt()`

```cpp
void halt()
```

Send the ISO 14443 HALT command to the active card, putting it in HALT state.
Also clears the MFRC522 crypto1 engine.  Call this after finishing with a card.

---

### <a id="rc522-common-helpers"></a>Common helpers

These two methods collapse the typical "log a card dump" workflow to a single call.

#### `printInfo()`

```cpp
void printInfo(Print &out = Serial);
```

Print `UID` / `ATQA` / `SAK` / type-name of the last detected card to the given
stream. Equivalent to calling the getters and printing each one manually:

```cpp
out.print(F("  UID:   ")); out.println(getUID());
out.print(F("  ATQA:  ")); out.println(getATQA());
out.print(F("  SAK:   0x")); out.println(getSAK(), HEX);
out.print(F("  Type:  ")); out.println(getCardTypeName());
```

#### `dumpToSerial()`

```cpp
uint8_t dumpToSerial(const uint8_t *key = nullptr);
```

Type-adaptive memory dump to the Serial Monitor.

| Card family   | Path used                                         |
|---------------|---------------------------------------------------|
| Classic 1K/4K/Mini | `dumpClassic(key ? key : NIUS_KEY_DEFAULT, printer)` |
| Ultralight / NTAG | `dumpUltralight(printer)`                            |
| Anything else     | prints `"not supported for this family"` and returns `0` |

Output format matches `MIFARE_DumpToSerial` from the standard library:

```
  0:  04 D9 F3 44 4B 80 44 00  00 00 00 00 00 00 00 00  [.........D K..D........]
  1:  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  [................]
  ...
```

Returns the number of sectors (Classic) or pages (Ultralight) successfully read.

**Pre-built constants:**

```cpp
extern const uint8_t NIUS_KEY_DEFAULT[6];   // {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
```

---

### `authenticate()`

```cpp
uint8_t authenticate(uint8_t blockAddr, uint8_t keyType, uint8_t *key)
```

Perform MIFARE Classic authentication for the sector containing `blockAddr`.
Must be called before `readBlock()` or `writeBlock()`.

| Parameter | Description |
|-----------|-------------|
| `blockAddr` | Block number to authenticate (0–63 for 1K, 0–255 for 4K) |
| `keyType` | `NIUS_KEY_A` (0x60) or `NIUS_KEY_B` (0x61) |
| `key` | Pointer to a 6-byte key array |

**Returns:** `NIUS_OK` on success, `NIUS_ERR_AUTH` on failure.

**Default factory key:** `{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}`  
A global constant `NIUS_KEY_DEFAULT[6]` is provided.

---

### `dumpClassic()`

```cpp
uint8_t dumpClassic(uint8_t *key, void (*printer)(uint8_t *))
```

Dump every block on a MIFARE Classic card whose sector authenticates with
`key`. The callback `printer` is invoked once per 16-byte block (4 blocks per
sector). Pass `nullptr` for `printer` to skip printing and just return the
count.

**Returns:** number of sectors that authenticated successfully.

For most cards, `dumpToSerial()` is more convenient — it picks the right
`dumpClassic` invocation for you.

---

### `setUid()`

```cpp
uint8_t setUid(uint8_t *newUid, uint8_t uidSize)
```

Rewrite block 0 of a MIFARE Classic card with a new UID. Authenticates sector
0 with `NIUS_KEY_DEFAULT` (Key A), then writes the new block 0. On a **Chinese
magic card** (CUID / DirectWrite / FUID / Gen2) the write is accepted via a
backdoor; on a stock MIFARE Classic card the write is correctly rejected.

| Parameter | Description |
|-----------|-------------|
| `newUid`  | Pointer to the new UID bytes (4 bytes for 1K / Mini) |
| `uidSize` | Number of UID bytes (typically 4) |

**Returns:** `NIUS_OK` on success.

---

### `readBlock()`

```cpp
uint8_t readBlock(uint8_t blockAddr, uint8_t *data)
```

Read 16 bytes from a MIFARE Classic block.  The block's sector must be authenticated
first with `authenticate()`.

| Parameter | Description |
|-----------|-------------|
| `blockAddr` | Block number (must be authenticated) |
| `data` | Output buffer, exactly 16 bytes |

**Returns:** `NIUS_OK` on success.

---

### `writeBlock()`

```cpp
uint8_t writeBlock(uint8_t blockAddr, uint8_t *data)
```

Write 16 bytes to a MIFARE Classic block.  The block's sector must be authenticated
first with `authenticate()`.

| Parameter | Description |
|-----------|-------------|
| `blockAddr` | Block number (must be authenticated) |
| `data` | Input buffer, exactly 16 bytes |

**Returns:** `NIUS_OK` on success.

> **WARNING:** Do NOT write to block 0 (manufacturer data — read-only).  
> Do NOT write incorrect data to sector trailer blocks (3, 7, 11 … 63) as this can
> permanently lock the sector.

---

### `stopCrypto()`

```cpp
void stopCrypto()
```

Disable the MFRC522 crypto1 engine after MIFARE operations.  Call this before the
next `cardPresent()` scan, or the chip may refuse to communicate.

---

### `readPage()` / `writePage()`

```cpp
uint8_t readPage(uint8_t page, uint8_t *data)   // 4 pages (16 bytes)
uint8_t writePage(uint8_t page, uint8_t *data)  // 4 bytes to one page
```

Read / write on a **MIFARE Ultralight / NTAG** tag.  No authentication is
required (the Ultralight family is page-addressed, not sectored).  The
card must be present in the field and have been brought out of HALT
state with `cardPresentWake()`.

| Parameter | Meaning |
|-----------|---------|
| `page`    | page number (page 0 holds the first 4 UID bytes) |
| `data`    | for `readPage`: 16-byte receive buffer (4 pages); for `writePage`: 4-byte payload |

`readPage()` reads 4 consecutive pages (16 bytes) at a time, matching
the MIFARE Ultralight / NTAG spec — page 0 returns pages 0-3, page 4
returns pages 4-7, etc.  It stops on the first NAK (out of memory).

`writePage()` writes a single 4-byte page and returns `NIUS_OK` on the
4-bit ACK (`0xA`) or `NIUS_ERR_AUTH` on NAK.

These methods do **not** work on MIFARE Classic cards (which need the
`readBlock` / `writeBlock` flow with Key A or Key B authentication).

---

### `dumpUltralight()`

```cpp
uint8_t dumpUltralight(void (*printer)(uint8_t *))
```

Dump every page of a MIFARE Ultralight / NTAG card that responds. The
callback `printer` is invoked once per 4-page chunk (16 bytes). Stops on
the first NAK. Pass `nullptr` to skip printing.

**Returns:** the number of pages successfully read.

---

### `antennaOn()` / `antennaOff()`

```cpp
void antennaOn()
void antennaOff()
```

Enable or disable the 13.56 MHz RF antenna.

---

### `setAntennaGain()` / `getAntennaGain()`

```cpp
void    setAntennaGain(uint8_t gain)
uint8_t getAntennaGain()
```

Set or read the receiver gain.  Higher gain = longer read range but more noise.

| Constant | dBm | Notes |
|----------|-----|-------|
| `NIUS_GAIN_18DB` | 18 | Minimum |
| `NIUS_GAIN_23DB` | 23 | |
| `NIUS_GAIN_33DB` | 33 | |
| `NIUS_GAIN_38DB` | 38 | |
| `NIUS_GAIN_43DB` | 43 | |
| `NIUS_GAIN_48DB` | 48 | Maximum (default after `begin()`) |

---

### `setIRQPin()`

```cpp
void setIRQPin(uint8_t irqPin)
```

Configure the GPIO pin connected to the RC522 IRQ output (active LOW).
After calling this, attach an interrupt on the same pin in your sketch:

```cpp
rfid.setIRQPin(13);
attachInterrupt(digitalPinToInterrupt(13), myISR, FALLING);
```

---

### Raw Register Access

```cpp
uint8_t readRegister(uint8_t addr)
void    writeRegister(uint8_t addr, uint8_t value)
void    setRegisterBits(uint8_t addr, uint8_t mask)
void    clearRegisterBits(uint8_t addr, uint8_t mask)
```

Direct access to MFRC522 registers.  Register addresses are defined in
`NiusMFRC522_Reg.h` (e.g. `MFRC522_REG_VERSION` = 0x37).  Use these for features
not covered by the high-level API.

---

### <a id="rc522-debug"></a>Debug / diagnostics

Three high-level helpers for "the card isn't responding — is it me or the card?"

#### `printRegisters()`

```cpp
void printRegisters(Print &out = Serial);
```

Print the chip's configuration registers: `VersionReg`, `CommandReg`, `ModeReg`,
`TxASKReg`, `TModeReg` / `TPrescalerReg` / `TReloadReg`, and `TxControlReg` (with
the antenna on/off state spelled out). Use this when you want to know whether
the chip is configured correctly — wrong timer / TxControl values, missing
modulation, etc., are typical firmware problems.

#### `printStatus()`

```cpp
void printStatus(Print &out = Serial);
```

Print the post-transceive IRQ / ErrorReg / Status2Reg / FIFOLevelReg, and
decode any set bits in ErrorReg (`BufferOvfl`, `ParityErr`, `CollErr`,
`ProtocolErr`, ...) and ComIrqReg (`TimerIRq`, `RxIRq`, `TxIRq`, `IdleIRq`).
Use this right after a failed transceive to see *why* it failed.

#### `powerCycle()`

```cpp
void powerCycle(uint16_t holdMs = 1000);
```

Disable the antenna for `holdMs` milliseconds, then re-enable and SOFT_RESET
the chip (with the register re-init `reset()` already applies). On cards
whose protocol state has latched, the field-down period lets the
card-side capacitor discharge so the next interaction boots from a clean
state. Long power-cycles (>=30 s) are usually enough to partially recover
a stuck MIFARE Ultralight / CUID card on this hardware.

The companion sketch for diagnosing bricked cards is just a `loop()` that
combines these:

```cpp
if (!rfid.cardPresentWake()) return;
rfid.printRegisters();              // once at the top, then again per tag
rfid.printStatus();                 // after each failed transceive
rfid.powerCycle();                  // if a card returns AUTH/NAK gibberish
```

---

### Raw SPI Transceive

```cpp
NiusRC522::Status transceive(uint8_t *sendData, uint8_t sendLen,
                             uint8_t *backData, uint8_t *backLen,
                             bool      checkCRC = false)
```

Low-level wrapper around the MFRC522 FIFO. Send a buffer, receive a buffer,
optionally verify that the trailing 2 bytes are the ISO14443 CRC-A of the
preceding payload. Returns `NIUS_OK` on success or `NIUS_ERR_*` otherwise. The
maximum receive length is 18 bytes (16 payload + 2 CRC).

Use this as the building block for any custom MIFARE command.

---

### Public Fields

**Status:** Stub — full implementation in a future release.

### Constructors

```cpp
NiusNRF24L01 radio(cePin, csnPin);                          // Hardware SPI
NiusNRF24L01 radio(cePin, csnPin, sckPin, mosiPin, misoPin); // Software SPI
```

### Key Methods

| Method | Description |
|--------|-------------|
| `begin()` | Initialise the NRF24L01 |
| `setChannel(channel)` | Set RF channel 0–125 (2400+ch MHz) |
| `setDataRate(rate)` | `NIUS_NRF24_250KBPS` / `_1MBPS` / `_2MBPS` |
| `setPower(level)` | `NIUS_NRF24_PWR_MIN` … `NIUS_NRF24_PWR_MAX` |
| `openWritingPipe(addr)` | Set TX destination address (5 bytes) |
| `openReadingPipe(pipe, addr)` | Open RX pipe 0–5 |
| `startListening()` | Enter RX mode |
| `stopListening()` | Enter standby/TX mode |
| `available()` | Check if payload is waiting |
| `readRadio(buf, len)` | Read payload from RX FIFO |
| `writeRadio(buf, len)` | Transmit a payload |

---

## NiusHC12 — HC-12 Long-Range Serial

**Status:** Stub — full implementation in a future release.

### Constructor

```cpp
NiusHC12 hc12(serial, setPin, baudRate);
```

| Parameter | Description |
|-----------|-------------|
| `serial` | `HardwareSerial` reference (e.g. `Serial1`) |
| `setPin` | GPIO connected to HC-12 SET (AT mode control) |
| `baudRate` | UART baud rate (default 9600) |

### Key Methods

| Method | Description |
|--------|-------------|
| `send(data)` | Send a String over the air |
| `receive()` | Read received String |
| `available()` | Bytes waiting to read |
| `setChannel(ch)` | Channel 1–100 (433.4 + (ch-1)×0.4 MHz) |
| `setPower(level)` | Power 1–8 (−1 dBm to 20 dBm) |
| `setBaud(baud)` | Change UART baud (requires re-init) |
| `setMode(mode)` | `NIUS_HC12_FU1` … `NIUS_HC12_FU4` |
| `enterATMode()` | Pull SET LOW, enter AT command mode |
| `exitATMode()` | Release SET, return to transparent mode |
| `sendAT(cmd)` | Send raw AT command string |
| `readResponse(ms)` | Read AT response within timeout |

---

## NiusHC06 — HC-06 / HC-05 Bluetooth SPP

**Status:** Stub — full implementation in a future release.

### Constructor

```cpp
NiusHC06 bt(serial, baudRate);
```

### Key Methods

| Method | Description |
|--------|-------------|
| `send(data)` | Send String over Bluetooth |
| `receive()` | Read incoming String |
| `available()` | Bytes available to read |
| `setName(name)` | Set BT device name (AT command) |
| `setBaud(baud)` | Change UART baud (AT command) |
| `setPIN(pin)` | Set 4-digit pairing PIN |
| `getName()` | Query device name |
| `getAddress()` | Query BT MAC address |
| `isConnected()` | Check connection state (needs STATE pin) |
| `sendAT(cmd)` | Send raw AT command |
| `readResponse(ms)` | Read AT response |

---

## NiusPN532 — PN532 NFC/RFID

I2C (address `0x24`) and SPI. Detects ISO 14443A tags, authenticates /
reads / writes MIFARE Classic blocks, and reads / writes Type-2 NDEF.

### Constructors

```cpp
#if defined(ARDUINO_ARCH_SAMD)
  #define PN532_IRQ 9          // required on SAMD21
#else
  #define PN532_IRQ 0xFF       // optional elsewhere
#endif
NiusPN532 nfc(PN532_IRQ, 0xFF);
NiusPN532 nfc(Wire, PN532_IRQ, 0xFF);
NiusPN532 nfc(csPin, rstPin, true);   // SPI
```

| Platform | IRQ | Mechanism when IRQ unwired |
|---|---|---|
| SAMD21 | **Required** | — (status poll can hang `Wire`) |
| ESP32, RP2040, UNO R4… | Optional | `WIRE_HAS_TIMEOUT` → I2C status poll |

Pass `rstPin = 0xFF` when RSTO is unwired. SAMConfiguration enables the IRQ
line automatically when `irqPin != 0xFF` — for **both I2C and SPI** (I2C
constructor irq pin, or `setIRQPin()` before `begin()` on SPI).

### Key Methods

| Method | Description |
|--------|-------------|
| `begin()` | Reset, GetFirmwareVersion, SAMConfiguration |
| `setIRQPin(pin)` | Optional P70_IRQ (active LOW). Call **before** `begin()`; works for I2C **and** SPI |
| `setI2CClock(hz)` | Wire clock (default 100 kHz) |
| `setSpiClock(hz)` | SPI clock (default 2 MHz; clamped 100 kHz–4 MHz) |
| `setPassiveActivationRetries(n)` | `InListPassiveTarget` retry count (`0xFF` = forever) |
| `getFirmwareVersion(ver)` | Raw IC / Ver / Rev / Support word |
| `cardPresent()` | Detect ISO 14443A; sets `lastError` / `lastCardType` |
| `cardPresentWake()` | RF field off/on then `cardPresent()` — wakes HALT'd cards after `halt()` |
| `printInfo()` | Print UID / ATQA / SAK / Type |
| `dumpToSerial(key=nullptr)` | Type-adaptive dump (Classic sectors / UL pages); RC522-style lines |
| `getCardType()` / `getCardTypeName()` | `NIUS_CARD_*` (same as RC522) |
| `errorName(code)` | Flash string for `NIUS_OK` / `NIUS_ERR_*` |
| `getUID()` / `getUIDBytes()` | UID helpers |
| `getATQA()` / `getSAK()` | Last detection meta |
| `authenticate(block, keyType, key)` | Classic auth (`key=nullptr` → factory) |
| `readBlock(block, data)` | Read 16-byte block |
| `writeBlock(block, data, force=false)` | Write; refuses block 0 / trailers unless `force` |
| `setUid(uid, len, commit=false)` | Safe block-0 UID path (BCC + dry-run) |
| `readPage(page, data)` | Ultralight / NTAG READ — **16 bytes** (4 pages), same as RC522 |
| `writePage(page, data)` | Ultralight / NTAG WRITE — 4 bytes; refuses pages 0–3 |
| `halt()` / `stopCrypto()` | Release target / end crypto session |
| `readNDEF(buf, len)` | Read NDEF from Type 2 tag |
| `writeNDEF(buf, len)` | Write NDEF to Type 2 tag |

Public state: `lastError`, `lastCardType`, `uid[]`, `uidLen`.

### Quick start (I2C)

```cpp
#include <NiusWireless.h>

NiusPN532 nfc(PN532_IRQ, 0xFF);   // platform default from examples

void setup() {
    Serial.begin(9600);
    delay(1500);
    nfc.begin();
}

void loop() {
    if (!nfc.cardPresentWake()) return;
    nfc.printInfo();
    nfc.dumpToSerial();
    nfc.halt();
}
```

See `examples/pn532_i2c_basic` and `examples/pn532_i2c_adv`.

### Quick start (SPI)

```cpp
#include <NiusWireless.h>

NiusPN532 nfc(8, 0xFF, true);   // CS=D8 (M0-Mini), no RST GPIO; SPI mode

void setup() {
    Serial.begin(9600);
    delay(1500);
    nfc.setSpiClock(2000000UL);   // optional; default is 2 MHz
    // Optional IRQ (advanced): nfc.setIRQPin(9);  // BEFORE begin()
    nfc.begin();
}

void loop() {
    if (!nfc.cardPresentWake()) return;
    nfc.printInfo();
    nfc.halt();
}
```

Reference wiring (RobotDyn SAMD21 M0-Mini): ICSP SCK/MOSI/MISO, **SS=D8**,
optional IRQ=D9. Hardware SPI is used by default; soft SPI is a fallback.

- Scan: `examples/pn532_spi_scan` — begin + firmware, then wake/printInfo.
- Basic (no IRQ): `examples/pn532_spi_basic` — polls SPI status byte.
- Advanced (IRQ): `examples/pn532_spi_adv` — `setIRQPin()` + dump / Classic block R/W.

Elechouse DIP for SPI: **SW1=OFF, SW2=ON**. After changing DIP, apply an
RSTO / power-on edge (USB reconnect or RESET) so I0/I1 re-latch.

---

## Return Codes

All MIFARE operations return one of these `uint8_t` codes:

| Constant | Value | Meaning |
|----------|-------|---------|
| `NIUS_OK` | 0x00 | Success |
| `NIUS_ERR_NOTAG` | 0x01 | No card in range |
| `NIUS_ERR_TIMEOUT` | 0x02 | Operation timed out |
| `NIUS_ERR_CRC` | 0x03 | CRC mismatch |
| `NIUS_ERR_COLLISION` | 0x04 | Bit collision |
| `NIUS_ERR_AUTH` | 0x05 | Authentication failed |
| `NIUS_ERR_OVERFLOW` | 0x06 | Buffer overflow |
| `NIUS_ERR_PARAM` | 0x07 | Bad parameter |
| `NIUS_ERR_UNKNOWN` | 0xFF | Unclassified error |

---

## Card Type Constants

Returned by `NiusRC522::getCardType()` and `NiusPN532::getCardType()`:

| Constant | Description |
|----------|-------------|
| `NIUS_CARD_UNKNOWN` | Unidentified card |
| `NIUS_CARD_MIFARE_MINI` | MIFARE Mini (320 B, 5 sectors) |
| `NIUS_CARD_MIFARE_1K` | MIFARE Classic 1K |
| `NIUS_CARD_MIFARE_4K` | MIFARE Classic 4K |
| `NIUS_CARD_MIFARE_UL` | MIFARE Ultralight |
| `NIUS_CARD_MIFARE_PLUS` | MIFARE Plus |
| `NIUS_CARD_ISO14443_4` | ISO 14443-4 (smart card) |
| `NIUS_CARD_ISO18092` | ISO 18092 / NFC-IP1 |
| `NIUS_CARD_TNP3XXX` | NXP TNP3xxx |
| `NIUS_CARD_DESFIRE` | MIFARE DESFire |

---

## Wiring Quick-Reference

### RC522 — Arduino UNO R4 WiFi (software SPI)

| RC522 Pin | Arduino Pin | Notes |
|-----------|-------------|-------|
| SDA (CS) | SDA / D18 / A4 | Chip-select |
| SCK | SCL / D19 / A5 | Software SPI clock |
| MOSI | D11 | |
| MISO | D12 | |
| IRQ | D13 | Optional interrupt |
| RST | D10 | Reset |
| 3.3V | 3.3V | Do NOT use 5V |
| GND | GND | |

### RC522 — Standard hardware SPI wiring (any UNO-compatible board)

| RC522 Pin | Arduino Pin |
|-----------|-------------|
| SDA (CS) | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| RST | D9 |
| 3.3V | 3.3V |
| GND | GND |

```cpp
// Standard hardware SPI constructor
NiusRC522 rfid(10, 9);
```

### NRF24L01 — Typical wiring

| NRF24 Pin | Arduino Pin |
|-----------|-------------|
| CE | D9 |
| CSN | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| VCC | 3.3V |
| GND | GND |

> **Important:** The NRF24L01 is a 3.3 V device.  Never connect VCC to 5 V.
> On 5 V boards, add a 10 µF capacitor between VCC and GND close to the module.
