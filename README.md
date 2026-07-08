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

## Examples

| Sketch | Module | Description |
|--------|--------|-------------|
| `rc522_basic` | RC522 | Detect card UID and type |
| `rc522_adv` | RC522 | IRQ, raw register access, gain control |
| `rc522_s50` | RC522 | Read and write MIFARE Classic 1K blocks |
| `rc522_tag` | RC522 | Full keychain tag operations: dump, write, value block, key change |
| `rfm95_basic` | RFM95W | Send/receive LoRa packets |
| `rfm95_adv` | RFM95W | CAD, interrupt RX, full config |
| `sx1262_basic` | SX1262 | Send/receive with SX1262/SX1268 |
| `sx1262_adv` | SX1262 | DCDC, TCXO, DIO2 RF switch, CAD |
| `nrf24_basic` | NRF24L01 | Transmit/receive counter packets |
| `hc12_basic` | HC-12 | Wireless Serial Monitor bridge |
| `hc06_basic` | HC-06 | Bluetooth SPP terminal |
| `pn532_basic` | PN532 | NFC tag UID reader |

## License

This library is released under the MIT License.
