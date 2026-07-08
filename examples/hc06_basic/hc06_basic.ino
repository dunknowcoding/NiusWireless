/*
 * hc06_basic — HC-06 / HC-05 Bluetooth SPP example
 *
 * Connects the Arduino to a smartphone via Bluetooth (Serial Port Profile).
 * Messages received over Bluetooth are printed to the Serial Monitor, and
 * anything typed in the Serial Monitor is sent back over Bluetooth.
 *
 * Pair the HC-06 in your phone's Bluetooth settings (default PIN: 1234).
 * Then use a Bluetooth terminal app (e.g. Serial Bluetooth Terminal on
 * Android) to communicate with this sketch.
 *
 * --- Wiring ---
 *   HC-06 VCC -> 5V
 *   HC-06 GND -> GND
 *   HC-06 TXD -> D0  (RX on UNO — or use Serial1 RX on multi-UART boards)
 *   HC-06 RXD -> D1  (TX on UNO — use a voltage divider to 3.3 V!)
 *
 * On UNO R4 WiFi, Mega, ESP32 etc., use Serial1:
 *   HC-06 TXD -> Serial1 RX pin
 *   HC-06 RXD -> Serial1 TX pin  (add 1kΩ + 2kΩ voltage divider)
 *
 * NOTE: The HC-06 driver is a stub in this release.
 *       This sketch compiles but does not communicate until the full
 *       driver is available. Check the NiusWireless releases.
 */

#include <NiusWireless.h>

// Hardware UART: Serial1 is recommended when available (avoids USB conflict)
NiusHC06 bt(Serial1, 9600);

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(10); }

    Serial.println("NiusWireless — HC-06 Basic Example");
    Serial.println("-----------------------------------");

    if (!bt.begin()) {
        Serial.println("WARNING: HC-06 not detected.");
        Serial.println("         Check wiring and baud rate.");
    }

    Serial.println("HC-06 ready. Pair on your phone, then open a BT terminal.");
    Serial.println("Type here -> sends to phone | Phone types -> shows here.");
    Serial.println();
}

void loop() {
    // Serial Monitor -> Bluetooth
    if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        msg.trim();
        if (msg.length() > 0) {
            bt.send(msg);
            Serial.print("Sent to BT: ");
            Serial.println(msg);
        }
    }

    // Bluetooth -> Serial Monitor
    if (bt.available()) {
        String incoming = bt.receive();
        if (incoming.length() > 0) {
            Serial.print("BT received: ");
            Serial.println(incoming);
        }
    }
}
