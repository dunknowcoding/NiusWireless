/*
 * NiusPN532.cpp — PN532 stub implementation
 *
 * --- Status: STUB — full implementation coming in a future release ---
 */

#include "NiusPN532.h"

NiusPN532::NiusPN532(uint8_t irqPin, uint8_t rstPin) {
    _irqPin  = irqPin;
    _rstPin  = rstPin;
    _csPin   = 0;
    _useSPI  = false;
    _ready   = false;
    uidLen   = 0;
    memset(uid, 0, sizeof(uid));
}

NiusPN532::NiusPN532(uint8_t csPin, uint8_t rstPin, bool useSPI) {
    _csPin   = csPin;
    _rstPin  = rstPin;
    _irqPin  = 0xFF;
    _useSPI  = useSPI;
    _ready   = false;
    uidLen   = 0;
    memset(uid, 0, sizeof(uid));
}

bool    NiusPN532::begin()                            { return false; }
bool    NiusPN532::isReady()                          { return false; }
void    NiusPN532::reset()                            { }
String  NiusPN532::getVersion()                       { return "PN532 (stub)"; }
bool    NiusPN532::cardPresent()                      { return false; }
String  NiusPN532::getUID()                           { return ""; }
bool    NiusPN532::getUIDBytes(uint8_t *, uint8_t &l) { l = 0; return false; }
uint8_t NiusPN532::authenticate(uint8_t, uint8_t, uint8_t *) { return NIUS_ERR_UNKNOWN; }
uint8_t NiusPN532::readBlock(uint8_t, uint8_t *)      { return NIUS_ERR_UNKNOWN; }
uint8_t NiusPN532::writeBlock(uint8_t, uint8_t *)     { return NIUS_ERR_UNKNOWN; }
bool    NiusPN532::readNDEF(uint8_t *, uint8_t &l)    { l = 0; return false; }
bool    NiusPN532::writeNDEF(uint8_t *, uint8_t)      { return false; }
