/*
 * NiusHC06.cpp — HC-06 / HC-05 stub implementation
 *
 * --- Status: STUB — full implementation coming in a future release ---
 */

#include "NiusHC06.h"

NiusHC06::NiusHC06(HardwareSerial &serial, uint32_t baudRate) {
    _hwSerial = &serial;
    _baudRate = baudRate;
    _ready    = false;
}

bool    NiusHC06::begin()                          { return false; }
bool    NiusHC06::isReady()                        { return false; }
void    NiusHC06::reset()                          { }
String  NiusHC06::getVersion()                     { return "HC-06 (stub)"; }
void    NiusHC06::send(String)                     { }
void    NiusHC06::sendBytes(uint8_t *, uint8_t)    { }
int     NiusHC06::available()                     { return 0; }
String  NiusHC06::receive()                        { return ""; }
int     NiusHC06::readByte()                       { return -1; }
bool    NiusHC06::setName(String)                  { return false; }
bool    NiusHC06::setBaud(uint32_t)                { return false; }
bool    NiusHC06::setPIN(String)                   { return false; }
String  NiusHC06::getName()                        { return ""; }
String  NiusHC06::getAddress()                     { return ""; }
bool    NiusHC06::isConnected()                    { return false; }
void    NiusHC06::sendAT(String)                   { }
String  NiusHC06::readResponse(uint32_t)           { return ""; }
