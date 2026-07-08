# NiusWireless

An expandable Arduino library for wireless and RF modules.

Provides a clean, Arduino-style API for RFID, radio, and Bluetooth modules
with wide board support.

## Supported Modules

| Module | Class | Status | Protocol |
|--------|-------|--------|----------|
| RFID-RC522 (MFRC522) | `NiusRC522` | **Full** | SPI (hw + sw) |
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

| Sketch | Description |
|--------|-------------|
| `rc522_basic` | Detect a card, print UID and type |
| `rc522_adv` | IRQ mode, raw register access, gain control |
| `rc522_s50` | Read and write MIFARE Classic 1K (S50) blocks |
| `nrf24_basic` | Transmit / receive counter packets |
| `hc12_basic` | Wireless Serial Monitor bridge |
| `hc06_basic` | Bluetooth SPP terminal |
| `pn532_basic` | NFC tag UID reader |

## License

This library is released under the MIT License.
