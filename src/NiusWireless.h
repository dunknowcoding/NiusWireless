/*
 * NiusWireless.h — Top-level include for the NiusWireless library
 *
 * Include this single header in your sketch to access all supported modules.
 *
 * Quick-start example (RC522, software SPI):
 *
 *   #include <NiusWireless.h>
 *
 *   // CS=SDA(A4), RST=10, SCK=SCL(A5), MOSI=11, MISO=12
 *   NiusRC522 rfid(SDA, 10, SCL, 11, 12);
 *
 *   void setup() {
 *     Serial.begin(9600);
 *     rfid.begin();
 *   }
 *   void loop() {
 *     if (rfid.cardPresent()) {
 *       Serial.println(rfid.getUID());
 *       rfid.halt();
 *     }
 *   }
 */

#ifndef NIUS_WIRELESS_H
#define NIUS_WIRELESS_H

#include "NiusBase.h"

/* ---- Module headers -------------------------------------------------- */
#include "modules/RC522/NiusRC522.h"
#include "modules/NRF24L01/NiusNRF24L01.h"
#include "modules/HC12/NiusHC12.h"
#include "modules/HC06/NiusHC06.h"
#include "modules/PN532/NiusPN532.h"

#endif // NIUS_WIRELESS_H
