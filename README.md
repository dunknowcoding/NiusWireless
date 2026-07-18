# NiusWireless

> A unified, cross-platform Arduino driver for the wireless and RF modules you
> actually wire up to a microcontroller — RFID/NFC, LoRa, sub-GHz radios,
> BLE/Serial Bluetooth, 2.4 GHz packets — under one consistent API.

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Arduino Library](https://img.shields.io/badge/Arduino-Library-00979D.svg?logo=arduino&logoColor=white)](https://www.arduino.cc/reference/en/libraries/)
![Version 0.1.0](https://img.shields.io/badge/version-0.1.0-success.svg)
![Architectures: AVR • SAMD • ESP • nRF52 • STM32 • RP2040 • RA](https://img.shields.io/badge/architectures-AVR%20%E2%80%A2%20SAMD%20%E2%80%A2%20ESP%20%E2%80%A2%20nRF52%20%E2%80%A2%20STM32%20%E2%80%A2%20RP2040%20%E2%80%A2%20RA-blueviolet.svg)
![Maintained by NiusRobotLab](https://img.shields.io/badge/maintainer-NiusRobotLab-orange.svg)
[![Repo](https://img.shields.io/badge/github-dunknowcoding%2FNiusWireless-181717.svg?logo=github)](https://github.com/dunknowcoding/NiusWireless)

---

## Why NiusWireless?

Most Arduino wireless libraries solve one module — RC522 *or* LoRa *or* NRF24,
never all three with the same coding style, the same error model, or the same
naming. **NiusWireless** consolidates eight popular RF modules behind a single
header (`NiusWireless.h`) and a shared base class (`NiusBase`), so the sketch
you write for an RC522 over SPI on an UNO R4 looks the same as the one you
write for an SX1262 over SPI on a Pico W.

Highlights:

- **One API surface, eight modules.** RFID-RC522, PN532 NFC, RFM95/96/98,
  SX1261/62/68, RA-01/02, NRF24L01(+), HC-12, HC-05/06.
- **RC522 done right.** Hardware SPI, software (bit-bang) SPI **and** I2C;
  full card-type coverage (Classic 1K / 4K / Mini, Ultralight, NTAG213/215/216,
  Plus SL1); built-in CUID / UFUID / FUID magic-tag handling with a safe
  dry-run preview before any write hits block 0.
- **Honest typing.** `getCardType()` / `getCardTypeName()` tell you whether
  the tag you're holding is actually a Classic 1K or an Ultralight — and the
  driver picks the right read path (`dumpClassic` vs `dumpUltralight`).
- **Cross-platform.** AVR, SAMD, ESP32, ESP8266, nRF52 (ArduinoNRF),
  STM32, RP2040 / RP2350, Renesas RA — the same source builds on all of them.
- **Single header for sketches.** `#include <NiusWireless.h>` pulls in
  whichever module classes you reference.

---

## Table of contents

1. [Supported modules](#supported-modules)
2. [Quick start](#quick-start)
3. [Wiring reference](#wiring-reference)
4. [RC522 — card & tag coverage](#rc522--card--tag-coverage)
5. [Examples](#examples)
6. [Supported boards](#supported-boards)
7. [Installation](#installation)
8. [Documentation](#documentation)
9. [Contributing](#contributing)
10. [License](#license)

---

## Supported modules

| Module | Class(es) | Status | Bus |
|---|---|---|---|
| RFID-RC522 / MFRC522 | `NiusRC522` | Full | SPI (hw + sw), I2C |
| PN532 NFC/RFID | `NiusPN532` | Full | I2C, SPI |
| RFM95W / RFM96W / RFM98W | `NiusRFM95` / `NiusRFM96` / `NiusRFM98` | Full | SPI |
| RA-01 / RA-02 (SX1278) | `NiusRA01` / `NiusRA02` | Full | SPI |
| SX1261 / SX1262 / SX1268 | `NiusSX1261` / `NiusSX1262` / `NiusSX1268` | Full | SPI |
| NRF24L01 / NRF24L01+ | `NiusNRF24L01` | Full | SPI (hw + sw) |
| HC-12 long-range serial | `NiusHC12` | Full | UART |
| HC-06 / HC-05 Bluetooth | `NiusHC06` | Full | UART |

All classes derive from `NiusBase`, so `begin()` / `isReady()` / `reset()` /
`getVersion()` are always present.

---

## Quick start

### 1. Software SPI (any 5 GPIOs)

The reference wiring for an **Arduino UNO R4 WiFi** uses SDA / SCL as
software-SPI pins because they're conveniently broken out next to the
analog header. Any five GPIO pins work on any board.

```cpp
#include <NiusWireless.h>

// SDA(CS)=D18/A4  SCK=D19/A5  MOSI=D11  MISO=D12  RST=D10
NiusRC522 rfid(SDA, 10, SCL, 11, 12);

void setup() {
    Serial.begin(9600);
    delay(1500);          // USB-CDC enumerate on UNO R4
    rfid.begin();
}

void loop() {
    if (!rfid.cardPresent()) return;   // single-shot per physical tap
    rfid.printInfo();                  // UID / ATQA / SAK / type
    rfid.halt();                       // tag returns to HALT state
    delay(1000);                       // debounce
}
```

### 2. Hardware SPI (board's default SPI bus)

```cpp
#include <NiusWireless.h>

// csPin = 10, rstPin = 9 on a classic Arduino UNO
NiusRC522 rfid(10, 9);

void setup() {
    Serial.begin(9600);
    rfid.begin();                      // uses SPI.h defaults
}

void loop() {
    if (rfid.cardPresent()) {
        Serial.println(rfid.getUID());
        rfid.halt();
    }
}
```

### 3. I2C

```cpp
#include <NiusWireless.h>

#define RC522_I2C_ADDR 0x28   // 0x29 if I2C_ADD0 is tied HIGH on the board
#define RC522_RST_PIN  10

NiusRC522 rfid(Wire, RC522_I2C_ADDR, RC522_RST_PIN);

void setup() {
    Serial.begin(9600);
    rfid.begin();
}

void loop() {
    if (!rfid.cardPresent()) return;
    rfid.printInfo();
    rfid.halt();
}
```

> RC522 I2C is supported natively — the chip must be wired for I2C mode
> (I2C / SPI select pin tied appropriately) and use the chip's
> I2C_SDA / I2C_SCL pins.

---

## Wiring reference

### RC522 — software SPI (default)

| RC522 pin | UNO R4 WiFi | Notes |
|---|---|---|
| SDA (CS)  | D18 / A4  | Any GPIO works |
| SCK       | D19 / A5  | Any GPIO works |
| MOSI      | D11       | Any GPIO works |
| MISO      | D12       | Any GPIO works |
| IRQ       | D13       | Optional — see `setIRQPin()` |
| RST       | D10       | Any GPIO works |
| 3.3V      | 3.3V      | **Do not power from 5V** |
| GND       | GND       | |

### RC522 — I2C

| RC522 pin | Board pin | Notes |
|---|---|---|
| I2C_SDA | board SDA | default address 0x28 (0x29 with I2C_ADD0 HIGH) |
| I2C_SCL | board SCL | |
| RST     | any GPIO  | |
| 3.3V    | 3.3V      | |
| GND     | GND       | |

### RFM9x / SX127x (LoRa)

Standard SPI wiring — see your board's SPI defaults. RFM95 / RFM96 / RFM98 and
the RA-01 / RA-02 clones all share the same pinout and command set.

### SX1261 / SX1262 / SX1268

Standard SPI plus BUSY, DIO1, and optional DIO2 (RF switch) / DIO3 (TCXO).
See `examples/sx1262_basic` for a working configuration.

### NRF24L01(+)

SPI: SCK / MOSI / MISO / CSN / CE. Power from **3.3V**, not 5V. Add a 10 µF
cap across VCC / GND if you see brownouts on TX.

### HC-12 / HC-05 / HC-06

Plain UART — TX / RX cross-connected at 3.3V levels. Use a divider if your
board is 5V.

### PN532 — I2C (all supported boards)

| PN532 pin | M0-Mini | ESP32 / others | Notes |
|---|---|---|---|
| SDA | D20 | board default SDA | `Wire` |
| SCL | D21 | board default SCL | `Wire` |
| IRQ | **D9 (required)** | optional | see below |
| VCC | 3V3 | 3V3 | |
| GND | GND | GND | |

**IRQ wiring by MCU:**

| MCU | IRQ required? | Default in examples |
|---|---|---|
| SAMD21 (M0-Mini, Zero) | **Yes** — `Wire` has no clock-stretch timeout | D9 |
| ESP32, RP2040, UNO R4, AVR… | **No** — driver polls I2C status when `PN532_IRQ` is `0xFF` | unwired OK |

Examples pick the default automatically. Override before compile if needed:

```cpp
#define PN532_IRQ 4          // your GPIO, any board
#include ...                // or edit the #if block in the sketch
```

Test flow: `pn532_i2c_scan` → `pn532_i2c_basic` → `pn532_i2c_adv`.
Do not generic-scan address `0x24`.

`NiusPN532` mirrors the RC522 card/error surface: `getCardType()` /
`getCardTypeName()`, `lastError` + `errorName()`, protected
`writeBlock(..., force=false)`, and `setUid(..., commit=false)` dry-run
(BCC recomputed; manufacturer bytes preserved).

### PN532 — SPI (RobotDyn SAMD21 M0-Mini)

Per `SAMD21-M0-Mini.pdf`:

| PN532 pin | M0-Mini | Notes |
|---|---|---|
| SCK / MOSI / MISO | **ICSP** | Zero core pins 24 / 23 / 22 |
| SS / NSS | **D8** | Chip select |
| IRQ | **D9** | Required in `pn532_spi_adv`; unwired in basic |
| RSTO | **board RESET** | Not driven as a sketch GPIO (`rstPin = 0xFF`) |
| VCC | 3V3 | |
| GND | GND | |

Set DIP switches to SPI (Elechouse: **SW1=OFF, SW2=ON**).

- **`pn532_spi_basic`** — status-byte poll only (no IRQ).
- **`pn532_spi_adv`** — `setIRQPin(D9)` before `begin()`.

---

## RC522 — card & tag coverage

The `NiusRC522` driver detects the card family from the ATQA + SAK bytes that
anti-collision returns. For Ultralight-family cards it additionally sends
`GET_VERSION (0x60)` to distinguish the specific product.

| Detected type | SAK | ATQA | Block / page | Operated by NiusRC522 |
|---|---|---|---|---|
| MIFARE Mini          | 0x09      | 0x0400 | 16 B / 16 B | Yes |
| MIFARE Classic 1K    | 0x08      | 0x0400 | 16 B / 16 B | Yes |
| MIFARE Classic 4K    | 0x18      | 0x0200 | 16 B / 16 B | Yes |
| MIFARE Ultralight    | 0x00      | 0x4400 |  4 B / 16 B | Yes (`readPage` / `writePage`) |
| NTAG213 / 215 / 216  | 0x00      | 0x4400 |  4 B / 16 B | Yes (distinguished by `GET_VERSION`) |
| MIFARE Plus          | 0x10 / 0x11 | 0x4200 | 16 B / 16 B | Partial — SL1 only (no SL2/SL3) |
| MIFARE DESFire EV1   | 0x20 / 0x28 | 0x4400 | n/a | No — needs APDU (use PN532) |
| ISO 14443-4 generic  | 0x20+     | varies | n/a | No |
| ISO 18092 (NFC-IP1)  | 0x40+     | varies | n/a | No (peer-to-peer) |
| TNP3xxx (SmartMX)    | 0x30+     | varies | n/a | No |
| FeliCa (Type F)      | n/a       | n/a    | n/a | No — RC522 cannot read FeliCa |

### Chinese "UID card" / "magic card" variants

The driver recognises every Chinese UID-changeable card variant as
`NIUS_CARD_MIFARE_1K` — that's the protocol level. The differences live in
how the UID is rewritten:

| Variant | SAK | UID change mechanism | Operated by NiusRC522 |
|---|---|---|---|
| Gen 1a / Gen 1b ("UID card") | 0x08 | Special backdoor command (HALT + 0x40) | Detected as Classic 1K; UID change needs raw transceive — use a Proxmark3 |
| Gen 2 / CUID / DirectWrite  | 0x08 | Standard `WRITE 0xA0` to block 0    | Yes — `rc522_spi_tag` step 0 |
| FUID (Write-Once)            | 0x08 | Standard `WRITE 0xA0` to block 0, once | Yes — `rc522_spi_tag` step 0 (one-shot) |
| UFUID (Unfused FUID)         | 0x08 | Standard `WRITE 0xA0` to block 0    | Yes — `rc522_spi_tag` step 0 |
| Gen 3 / "Magic Gen3"         | 0x08 / 0x18 | Special gen3 backdoor (UID-only or full-block write) | Detected as Classic 1K / 4K; needs Proxmark3 or PN532 |
| Gen 4 / "Ultimate Magic"     | config | Configurable — emulates any of the above | Yes — depending on emulated type |
| Magic DesFire                 | 0x20 | APDU-level | No — needs PN532 |

The default factory key for all of these is `FF FF FF FF FF FF` on a fresh
card; later variants keep the factory key on sector 0 but switch the rest
to a custom key.

### Safe UID rewriting (CUID / FUID / UFUID)

Block 0 is special — a single bad write bricks the tag. The driver enforces
two safety rails:

- `writeBlock(block, data)` refuses **block 0** and **sector trailers**
  (blocks 3, 7, 11, 15, …) by default. Pass `force = true` to opt in.
- `setUid(newUid, uidSize)` is a **dry-run preview** by default — it prints
  the old UID, the new UID, the old / new BCC and which manufacturer bytes
  it preserves, then returns without writing. Pass `commit = true` to
  actually rewrite block 0; the call then backs up, writes, halts, re-detects
  and verifies the UID matches, and returns `NIUS_ERR_UNKNOWN` if the card
  reports a different UID.

```cpp
uint8_t newUid[4] = {0xDE, 0xAD, 0xBE, 0xEF};
rfid.setUid(newUid, 4);                // preview — no write
rfid.setUid(newUid, 4, /*commit=*/true); // backup, write, verify
```

---

## Examples

Every sketch is a self-contained `.ino` under `examples/`. Open it from the
Arduino IDE via **File → Examples → NiusWireless → …**.

| Sketch | Module | What it does |
|---|---|---|
| `rc522_spi_basic` | RC522 (software SPI) | Minimal UID / type dump — 30 lines, the smallest sketch that exercises the driver |
| `rc522_spi_adv`   | RC522 (software SPI) | IRQ, raw register access, gain control |
| `rc522_spi_s50`   | RC522 (software SPI) | Read and write MIFARE Classic 1K blocks |
| `rc522_spi_tag`   | RC522 (software SPI) | Auto-adapts to detected type — Classic dump / write / value / key change / CUID, Ultralight page flow, or "use a different tool" |
| `rc522_i2c_basic` | RC522 (I2C) | Minimal UID / type dump over I2C |
| `rc522_i2c_adv`   | RC522 (I2C) | IRQ, raw register access, gain control over I2C |
| `rfm95_basic`     | RFM95W | Send / receive LoRa packets |
| `rfm95_adv`       | RFM95W | CAD, interrupt RX, full config |
| `sx1262_basic`    | SX1262 | Send / receive with SX1262 / SX1268 |
| `sx1262_adv`      | SX1262 | DCDC, TCXO, DIO2 RF switch, CAD |
| `nrf24_basic`     | NRF24L01 | Transmit counter packets |
| `hc12_basic`      | HC-12 | Wireless Serial Monitor bridge |
| `hc06_basic`      | HC-06 | Bluetooth SPP terminal |
| `pn532_i2c_scan`  | PN532 (I2C) | Step 1: confirm chip at 0x24 via `begin()` |
| `pn532_i2c_basic` | PN532 (I2C) | Minimal UID / ATQA / SAK dump |
| `pn532_i2c_adv`   | PN532 (I2C) | Bus clock, retries, Classic block 4 read / write |
| `pn532_spi_basic` | PN532 (SPI) | Minimal UID dump — SPI status poll, no IRQ |
| `pn532_spi_adv`   | PN532 (SPI) | IRQ via `setIRQPin()`, retries, Classic block 4 R/W |

---

## Supported boards

The library is `architectures=*` in `library.properties` and builds cleanly on
every Arduino core we have tested:

| Family | Examples |
|---|---|
| AVR | UNO, Mega, Nano, Pro Mini |
| SAMD | Zero, MKR family, Nano 33 IoT |
| ESP32 | DevKitC, ESP32-S3, ESP32-C3 |
| ESP8266 | NodeMCU, Wemos D1 mini |
| nRF52 | ArduinoNRF core — ProMicro, nice!nano, SuperMini, XIAO nRF52 |
| STM32 | STM32duino — Blue Pill, Black Pill, Nucleo |
| RP2040 / RP2350 | Pico, Pico W, Pico 2 |
| Renesas RA | Arduino UNO R4 WiFi, UNO R4 Minima |

---

## Installation

### Arduino IDE (recommended)

1. Download the latest release `.zip` from
   [Releases](https://github.com/dunknowcoding/NiusWireless/releases).
2. In the IDE: **Sketch → Include Library → Add .ZIP Library…** and pick the
   downloaded file.
3. Restart the IDE. The examples appear under
   **File → Examples → NiusWireless**.

### Arduino CLI

```bash
arduino-cli lib install "NiusWireless"
```

### PlatformIO

```ini
lib_deps =
    https://github.com/dunknowcoding/NiusWireless.git
```

### Manual / Git checkout

Drop the `NiusWireless/` folder into your Arduino `libraries/` directory and
restart the IDE.

---

## Documentation

- **[docs/API.md](docs/API.md)** — full API reference, with wiring tables,
  constructor variants, error-code catalogue and per-method notes.
- **Header doc-comments** — every public method in `NiusRC522.h`,
  `NiusRFM9x.h`, etc. is documented inline. Most IDEs surface those tooltips
  automatically.

---

## Contributing

Issues and pull requests are welcome. A few guidelines:

- **One module per PR.** RC522 changes go in one PR, LoRa in another —
  keeps the review focused.
- **Run the examples on real hardware before submitting.** Hardware-SPI,
  software-SPI and I2C each have their own regressions.
- **Match the existing style.** `clang-format` config is at the repo root;
  public API follows the `NiusBase` interface — derive from it, don't fork
  it.

For security-relevant issues (e.g. an unintended write path on block 0), please
open a private advisory rather than a public issue.

---

## Maintainer

NiusRobotLab — Arduino and embedded firmware, mostly around RFID, LoRa and
2.4 GHz radios. Maintained by **dunknowcoding**.

---

## License

This library is released under the **MIT License**. See [`LICENSE`](LICENSE)
for the full text.

```
MIT License

Copyright (c) 2024-2026 dunknowcoding / NiusRobotLab

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```