/*
 * hc12_basic — HC-12 long-range serial transceiver example
 *
 * Sends text typed in the Serial Monitor out over the HC-12 radio link,
 * and prints anything received from another HC-12 to the Serial Monitor.
 *
 * Run this sketch on two boards (both wired to an HC-12 module) to
 * create a wireless serial link.
 *
 * --- Wiring ---
 *   HC-12 VCC -> 5V  (or 3.3V)
 *   HC-12 GND -> GND
 *   HC-12 TXD -> D2  (connect to MCU RX, use SoftwareSerial on AVR)
 *   HC-12 RXD -> D3  (connect to MCU TX)
 *   HC-12 SET -> D4  (AT command mode control)
 *
 * On boards with multiple hardware UARTs (UNO R4 WiFi, Mega, ESP32, etc.)
 * change the serial object to Serial1 and use the correct pins.
 *
 * NOTE: The HC-12 driver is a stub in this release.
 *       This sketch compiles but does not transmit until the full driver
 *       is available. Check the NiusWireless releases.
 */

#include <NiusWireless.h>

#define HC12_SET_PIN  4

// Hardware UART constructor: use Serial1 on boards that have it.
// Change to Serial2 / Serial3 as needed.
NiusHC12 hc12(Serial1, HC12_SET_PIN, 9600);

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — HC-12 Basic Example");
    Serial.println("-----------------------------------");

    if (!hc12.begin()) {
        Serial.println("WARNING: HC-12 not found or AT mode error.");
        Serial.println("         Continuing anyway (check wiring).");
    }

    Serial.println("HC-12 ready.");
    Serial.print("Version: ");
    Serial.println(hc12.getVersion());

    Serial.println("Type a message and press Enter to send it.");
    Serial.println();
}

void loop() {
    // Forward anything typed in Serial Monitor to the HC-12
    if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        msg.trim();
        if (msg.length() > 0) {
            hc12.send(msg);
            Serial.print("Sent: ");
            Serial.println(msg);
        }
    }

    // Print anything received from the other HC-12
    if (hc12.available()) {
        String incoming = hc12.receive();
        if (incoming.length() > 0) {
            Serial.print("Received: ");
            Serial.println(incoming);
        }
    }
}
