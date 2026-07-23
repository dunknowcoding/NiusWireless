/*
 * nrf24_dual_link — two NRF24L01 radios on one board, both directions
 *
 * Most NRF24 examples need two boards. A microcontroller with two SPI
 * peripherals can instead drive two radios itself, which makes a complete
 * transmit/receive link testable on a single board — useful for bring-up,
 * regression testing, and verifying wiring before deploying a real link.
 *
 * This sketch sends packets A->B, then B->A, and reports the result.
 *
 * --- Wiring (Raspberry Pi Pico / Pico 2, arduino-pico core) ---
 *
 *   Radio A on SPI0            Radio B on SPI1
 *     SCK  -> GP18               SCK  -> GP10
 *     MOSI -> GP19               MOSI -> GP11
 *     MISO -> GP16               MISO -> GP8
 *     CSN  -> GP20               CSN  -> GP7
 *     CE   -> GP21               CE   -> GP9
 *     IRQ  -> GP22 (unused)      IRQ  -> GP5 (unused)
 *
 *   Both radios: VCC -> 3V3 (never 5 V), GND -> GND.
 *   A 10 uF capacitor across each module's VCC/GND is strongly recommended;
 *   NRF24 modules brown out on transmit bursts without one.
 *
 * On other boards, change the pin numbers and SPI instances to match. Any
 * board with two usable SPI peripherals works the same way.
 */

#include <NiusWireless.h>

/* Radio A uses the default SPI bus, radio B the second one. */
NiusNRF24L01 radioA(21, 20);        // CE, CSN
NiusNRF24L01 radioB(9, 7, SPI1);    // CE, CSN, explicit bus

/* Addresses: A listens on "NODEA", B listens on "NODEB". */
uint8_t addrA[5] = {'N', 'O', 'D', 'E', 'A'};
uint8_t addrB[5] = {'N', 'O', 'D', 'E', 'B'};

static const uint8_t PACKETS = 10;
static const uint8_t CHANNEL = 76;

/* Send `count` packets from `tx` to `rx` and return how many arrived intact. */
uint8_t runLink(NiusNRF24L01 &tx, uint8_t *txTarget,
                NiusNRF24L01 &rx, const char *label) {
    tx.stopListening();
    tx.openWritingPipe(txTarget);
    rx.startListening();

    uint8_t delivered = 0;
    for (uint8_t i = 0; i < PACKETS; ++i) {
        uint8_t payload[8] = {'N', 'I', 'U', 'S', i, 0, 0, 0};
        bool acked = tx.writeRadio(payload, sizeof(payload));

        /* Auto-acknowledge already proves the receiver heard the packet;
         * still read it back so the payload itself is verified. */
        uint8_t received[8] = {0};
        bool got = false;
        unsigned long deadline = millis() + 50;
        while (millis() < deadline) {
            if (rx.available() && rx.readRadio(received, sizeof(received))) { got = true; break; }
        }

        if (acked && got && received[0] == 'N' && received[4] == i) {
            ++delivered;
        }
        delay(5);
    }

    rx.stopListening();
    NIUS_SERIAL.print(label);
    NIUS_SERIAL.print(delivered);
    NIUS_SERIAL.print('/');
    NIUS_SERIAL.println(PACKETS);
    return delivered;
}

bool g_okA = false;
bool g_okB = false;

void setup() {
    NIUS_SERIAL.begin(115200);
    unsigned long waitUntil = millis() + 3000;
    while (!NIUS_SERIAL && millis() < waitUntil) { delay(10); }

    /* The arduino-pico core lets each SPI bus pick its pins before begin(). */
    SPI.setSCK(18);  SPI.setTX(19);  SPI.setRX(16);
    SPI1.setSCK(10); SPI1.setTX(11); SPI1.setRX(8);

    g_okA = radioA.begin();
    g_okB = radioB.begin();

    if (g_okA && g_okB) {
        /* Both ends must agree on channel, rate, and address width. */
        radioA.setChannel(CHANNEL);  radioB.setChannel(CHANNEL);
        radioA.setDataRate(NIUS_NRF24_1MBPS);
        radioB.setDataRate(NIUS_NRF24_1MBPS);
        radioA.setPower(NIUS_NRF24_PWR_MAX);
        radioB.setPower(NIUS_NRF24_PWR_MAX);
        radioA.setAddress(addrA, 5);
        radioB.setAddress(addrB, 5);
    }
}

/*
 * The report repeats so a USB-CDC board can be opened at any time and still
 * show a complete result; resetting such a board to catch a one-shot banner
 * would drop the serial connection being watched.
 */
void loop() {
    NIUS_SERIAL.println();
    NIUS_SERIAL.println("NiusWireless - NRF24L01 dual-radio link test");
    NIUS_SERIAL.println("-------------------------------------------");
    NIUS_SERIAL.print("radio A: ");
    NIUS_SERIAL.println(g_okA ? radioA.getVersion() : String("NOT DETECTED"));
    NIUS_SERIAL.print("radio B: ");
    NIUS_SERIAL.println(g_okB ? radioB.getVersion() : String("NOT DETECTED"));

    if (!g_okA || !g_okB) {
        NIUS_SERIAL.println("RESULT: FAIL - check 3.3 V supply, wiring, and SPI pins");
        delay(3000);
        return;
    }

    NIUS_SERIAL.print("dynamic payload: ");
    NIUS_SERIAL.println(radioA.hasDynamicPayload() ? "yes" : "no (fixed 32 B)");

    uint8_t aToB = runLink(radioA, addrB, radioB, "A -> B delivered: ");
    uint8_t bToA = runLink(radioB, addrA, radioA, "B -> A delivered: ");

    bool pass = (aToB == PACKETS) && (bToA == PACKETS);
    NIUS_SERIAL.print("RESULT: ");
    NIUS_SERIAL.println(pass ? "PASS" : "FAIL");
    delay(3000);
}
