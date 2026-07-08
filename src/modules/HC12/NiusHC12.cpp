/*
 * NiusHC12.cpp — HC-12 stub implementation
 *
 * --- Status: STUB — full implementation coming in a future release ---
 */

#include "NiusHC12.h"

NiusHC12::NiusHC12(HardwareSerial &serial, uint8_t setPin, uint32_t baudRate) {
    _hwSerial = &serial;
    _setPin   = setPin;
    _baudRate = baudRate;
    _atMode   = false;
    _ready    = false;
}

bool    NiusHC12::begin()                    { return false; }
bool    NiusHC12::isReady()                  { return false; }
void    NiusHC12::reset()                    { }
String  NiusHC12::getVersion()               { return "HC-12 (stub)"; }
void    NiusHC12::send(String)               { }
void    NiusHC12::sendBytes(uint8_t *, uint8_t) { }
int     NiusHC12::available()               { return 0; }
String  NiusHC12::receive(uint32_t)          { return ""; }
int     NiusHC12::readByte()                 { return -1; }
bool    NiusHC12::setChannel(uint8_t)        { return false; }
bool    NiusHC12::setPower(uint8_t)          { return false; }
bool    NiusHC12::setBaud(uint32_t)          { return false; }
bool    NiusHC12::setMode(uint8_t)           { return false; }
uint8_t NiusHC12::getChannel()              { return 0; }
bool    NiusHC12::enterATMode()              { return false; }
void    NiusHC12::exitATMode()               { }
void    NiusHC12::sendAT(String)             { }
String  NiusHC12::readResponse(uint32_t)     { return ""; }
