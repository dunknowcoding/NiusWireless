/*
 * NiusNRF24L01.cpp — NRF24L01 stub implementation
 *
 * --- Status: STUB — full implementation coming in a future release ---
 * All public methods return safe default values so that sketches that
 * include NiusWireless.h compile without errors even before the full
 * driver is available.
 */

#include "NiusNRF24L01.h"

NiusNRF24L01::NiusNRF24L01(uint8_t cePin, uint8_t csnPin) {
    _cePin   = cePin;
    _csnPin  = csnPin;
    _softSPI = false;
    _ready   = false;
}

NiusNRF24L01::NiusNRF24L01(uint8_t cePin, uint8_t csnPin,
                             uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin) {
    _cePin   = cePin;
    _csnPin  = csnPin;
    _sckPin  = sckPin;
    _mosiPin = mosiPin;
    _misoPin = misoPin;
    _softSPI = true;
    _ready   = false;
}

bool    NiusNRF24L01::begin()                            { return false; }
bool    NiusNRF24L01::isReady()                          { return false; }
void    NiusNRF24L01::reset()                            { }
String  NiusNRF24L01::getVersion()                       { return "NRF24L01 (stub)"; }
bool    NiusNRF24L01::setChannel(uint8_t)                { return false; }
bool    NiusNRF24L01::setDataRate(uint8_t)               { return false; }
bool    NiusNRF24L01::setPower(uint8_t)                  { return false; }
bool    NiusNRF24L01::setAddress(uint8_t *, uint8_t)     { return false; }
void    NiusNRF24L01::openWritingPipe(uint8_t *)         { }
void    NiusNRF24L01::openReadingPipe(uint8_t, uint8_t *){ }
void    NiusNRF24L01::startListening()                   { }
void    NiusNRF24L01::stopListening()                    { }
bool    NiusNRF24L01::available()                        { return false; }
bool    NiusNRF24L01::readRadio(uint8_t *, uint8_t)      { return false; }
bool    NiusNRF24L01::writeRadio(uint8_t *, uint8_t)     { return false; }
uint8_t NiusNRF24L01::getStatus()                        { return 0; }
bool    NiusNRF24L01::testCarrier()                      { return false; }
uint8_t NiusNRF24L01::readReg(uint8_t)                   { return 0; }
void    NiusNRF24L01::writeReg(uint8_t, uint8_t)         { }
uint8_t NiusNRF24L01::spiTransfer(uint8_t d)             { return _softSPI ? softTransfer(d) : d; }
uint8_t NiusNRF24L01::softTransfer(uint8_t)              { return 0; }
