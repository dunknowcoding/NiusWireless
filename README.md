# NiusWireless

An expandable Arduino library for wireless and RF modules.

Provides a clean, Arduino-style API for RFID, radio, and Bluetooth modules
with wide board support.

## Supported Modules

| Module | Class | Status | Protocol |
|--------|-------|--------|----------|
| RFID-RC522 (MFRC522) | `NiusRC522` | **Full** | SPI (hw + sw) |
| RFM95W / RFM96W / RFM98W | `NiusRFM95` / `NiusRFM96` / `NiusRFM98` | **Full** | SPI |
| RA-01 / RA-02 (SX1278) | `NiusRA01` / `NiusRA02` | **Full** | SPI |
| SX1261 / SX1262 / SX1268 | `NiusSX1261` / `NiusSX1262` / `NiusSX1268` | **Full** | SPI |
| NRF24L01 / NRF24L01+ | `NiusNRF24L01` | Stub | SPI (hw + sw) |
| HC-12 long-range serial | `NiusHC12` | Stub | UART |
| HC-06 / HC-05 Bluetooth | `NiusHC06` | Stub | UART |
| PN532 NFC/RFID | `NiusPN532` | Stub | I2C / SPI |

## Quick Start — RC522 (UNO R4 WiFi)

```cpp
#include <NiusWireless.h>

// SDA(CS)=D18/A4  SCK=D19/A5  MOSI=D11  MISO=D12  IRQ=D13  RST=D10
NiusRC522 rfid(SDA, 10, SCL, 11, 12);

void setup() {
    Serial.begin(9600);
    rfid.begin();
}

void loop() {
    if (rfid.cardPresent()) {
        Serial.println(rfid.getUID());
        rfid.halt();
    }
}
```

## Supported Boards

AVR (UNO, Mega, Nano), SAMD (Zero, MKR), ESP32, ESP8266,
nRF52 (ArduinoNRF package — ProMicro, nice!nano, SuperMini, XIAO …),
STM32, RP2040/RP2350 (Pico, Pico W), Renesas RA (Arduino UNO R4 WiFi / Minima).

## Documentation

See [docs/API.md](docs/API.md) for the full API reference, wiring tables, and
usage notes.

## Card & Tag Type Coverage (RC522)

The `NiusRC522` driver detects the card family from the ATQA + SAK that
anti-collision returns. For Ultralight-family cards it also sends
`GET_VERSION (0x60)` to identify the specific product.

| Detected type | SAK | ATQA | Block / page size | Operate? |
|---|---|---|---|---|
| MIFARE Mini        | 0x09 | 0x0400 | 16 B / 16 B | ✅ |
| MIFARE Classic 1K  | 0x08 | 0x0400 | 16 B / 16 B | ✅ |
| MIFARE Classic 4K  | 0x18 | 0x0200 | 16 B / 16 B | ✅ |
| MIFARE Ultralight  | 0x00 | 0x4400 | 4 B  / 16 B | ✅ `readPage` / `writePage` |
| NTAG213 / 215 / 216 | 0x00 | 0x4400 | 4 B  / 16 B | ✅ (distinguished by `GET_VERSION`) |
| MIFARE Plus        | 0x10 / 0x11 | 0x4200 | 16 B / 16 B | ⚠️ Classic SL1 only (no SL2/SL3) |
| MIFARE DESFire EV1 | 0x20 / 0x28 | 0x4400 | n/a | ❌ needs APDU (use PN532) |
| ISO 14443-4 generic | 0x20+ | varies | n/a | ❌ |
| ISO 18092 (NFC-IP1) | 0x40+ | varies | n/a | ❌ peer-to-peer |
| TNP3xxx (SmartMX)  | 0x30+ | varies | n/a | ❌ |
| FeliCa (Type F, 212 kbps) | n/a | n/a | n/a | ❌ RC522 cannot read FeliCa |

### Chinese "UID card" / "magic card" variants

The driver recognises every Chinese UID-changeable card variant as
`NIUS_CARD_MIFARE_1K` — that's the protocol level. The differences are
in how the UID is rewritten:

| Variant | SAK | UID change mechanism | Operated by NiusRC522? |
|---|---|---|---|
| Gen 1a / Gen 1b (a.k.a. "UID card") | 0x08 | Special backdoor command (HALT + 0x40) | ⚠️ detected as Classic 1K; UID change needs raw transceive (not in the public API — use a Proxmark3) |
| Gen 2 / CUID / DirectWrite | 0x08 | Standard `WRITE 0xA0` to block 0 | ✅ `rc522_tag` step 0 |
| FUID (Write-Once) | 0x08 | Standard `WRITE 0xA0` to block 0, once | ✅ `rc522_tag` step 0 (one-shot) |
| UFUID (Unfused FUID) | 0x08 | Standard `WRITE 0xA0` to block 0 | ✅ `rc522_tag` step 0 |
| Gen 3 / "Magic Gen3" | 0x08 / 0x18 | Special gen3 backdoor (UID-only or full-block write) | ⚠️ detected as Classic 1K / 4K; needs Proxmark3 or PN532 + special tool |
| Gen 4 / "Ultimate Magic Card" | config | Configurable — emulates any of the above | ✅ detected and operated depending on the emulated type |
| Magic DesFire | 0x20 | APDU-level | ❌ needs PN532 |

The default factory key for all of these is `0xFF 0xFF 0xFF 0xFF 0xFF 0xFF`
(used in sectors 0..15 on a fresh card; later variants keep the factory
key on sector 0 but switch the rest to a custom key).

## Examples

| Sketch | Module | Description |
|--------|--------|-------------|
| `rc522_basic` | RC522 | Type explorer — UID, ATQA, SAK, GET_VERSION for NTAG, recommended next sketch |
| `rc522_adv` | RC522 | IRQ, raw register access, gain control |
| `rc522_s50` | RC522 | Read and write MIFARE Classic 1K blocks |
| `rc522_tag` | RC522 | Auto-adapts to detected type: full Classic flow (dump, write, value, key change, CUID) **or** Ultralight page flow **or** "use a different tool" message |
| `rfm95_basic` | RFM95W | Send/receive LoRa packets |
| `rfm95_adv` | RFM95W | CAD, interrupt RX, full config |
| `sx1262_basic` | SX1262 | Send/receive with SX1262/SX1268 |
| `sx1262_adv` | SX1262 | DCDC, TCXO, DIO2 RF switch, CAD |
| `nrf24_basic` | NRF24L01 | Transmit counter packets |
| `hc12_basic` | HC-12 | Wireless Serial Monitor bridge |
| `hc06_basic` | HC-06 | Bluetooth SPP terminal |
| `pn532_basic` | PN532 | NFC tag UID reader |

## License

This library is released under the MIT License.
