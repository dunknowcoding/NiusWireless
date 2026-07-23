/*
 * nrf24_basic — NRF24L01 basic transceiver example
 *
 * Two boards are required: one configured as transmitter, one as receiver.
 * Change the ROLE define below for each board.
 *
 * --- Wiring ---
 *   NRF24 CE   -> D9
 *   NRF24 CSN  -> D10
 *   NRF24 SCK  -> D13  (hardware SPI)
 *   NRF24 MOSI -> D11
 *   NRF24 MISO -> D12
 *   NRF24 VCC  -> 3.3V  (do NOT connect to 5V)
 *   NRF24 GND  -> GND
 *
 * NOTE: Fit a 10 uF capacitor across the module's VCC/GND. NRF24 modules
 *       draw short current bursts on transmit and are unreliable without it.
 *       Supply 3.3 V only; the module is not 5 V tolerant.
 *
 *       Both boards must use the same channel, data rate, and address.
 *       writeRadio() returns true only when the receiver acknowledged.
 *
 *       To test a link without a second board, see nrf24_dual_link.
 */

#include <NiusWireless.h>

// Set to 0 for transmitter, 1 for receiver
#define ROLE  0

// Pipe address — both boards must use the same address
uint8_t pipeAddr[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};

// Hardware SPI: CE=D9, CSN=D10
NiusNRF24L01 radio(9, 10);

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — NRF24L01 Basic Example");
    Serial.println("--------------------------------------");

    if (!radio.begin()) {
        Serial.println("ERROR: NRF24L01 not found. Check wiring and 3.3 V supply.");
        while (1) { delay(500); }
    }

    radio.setChannel(76);               // 2476 MHz — avoid WiFi channels
    radio.setDataRate(NIUS_NRF24_1MBPS);
    radio.setPower(NIUS_NRF24_PWR_MAX);

    if (ROLE == 0) {
        // Transmitter
        radio.stopListening();
        radio.openWritingPipe(pipeAddr);
        Serial.println("Role: TRANSMITTER");
        Serial.println("Sending packets...");
    } else {
        // Receiver
        radio.openReadingPipe(1, pipeAddr);
        radio.startListening();
        Serial.println("Role: RECEIVER");
        Serial.println("Listening for packets...");
    }
}

void loop() {
    if (ROLE == 0) {
        // Transmit a counter value
        static uint32_t counter = 0;
        uint8_t payload[4];
        payload[0] = (counter >> 24) & 0xFF;
        payload[1] = (counter >> 16) & 0xFF;
        payload[2] = (counter >> 8)  & 0xFF;
        payload[3] =  counter        & 0xFF;

        bool ok = radio.writeRadio(payload, 4);
        if (ok) {
            Serial.print("Sent packet #");
            Serial.println(counter);
        } else {
            Serial.println("Send failed (no ACK).");
        }
        counter++;
        delay(1000);

    } else {
        // Receive
        if (radio.available()) {
            uint8_t buf[4];
            radio.readRadio(buf, 4);
            uint32_t val = ((uint32_t)buf[0] << 24)
                         | ((uint32_t)buf[1] << 16)
                         | ((uint32_t)buf[2] << 8)
                         |  (uint32_t)buf[3];
            Serial.print("Received packet #");
            Serial.println(val);
        }
    }
}
