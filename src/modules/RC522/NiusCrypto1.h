// NiusCrypto1.h — public access to the MIFARE Classic Crypto1 cipher
// used by key-recovery attacks. The implementation lives inside
// NiusRC522.cpp (in the niusCrypto1 namespace) to keep the build
// graph simple.
//
// You only need this header if you're writing sketches that talk
// directly to the cipher (e.g. nested-authentication trace
// collection). For typical read/write/dump usage, the high-level
// NiusRC522::transceive / transceiveRaw APIs are sufficient.

#ifndef NIUS_CRYPTO1_H
#define NIUS_CRYPTO1_H

#include <stdint.h>

namespace niusCrypto1 {

// 48-bit LFSR state of the cipher, held as two 32-bit halves (odd / even).
struct State {
    uint32_t odd;
    uint32_t even;
};

// Initialize from a 6-byte key.
void init(State *s, const uint8_t key[6]);

// Initialize from a 48-bit key.
void init(State *s, uint64_t key);

// One cipher clock. in = input bit; is_encrypted = 1 to decrypt, 0 to encrypt.
// Returns the keystream bit for this round.
uint8_t clkBit(State *s, uint8_t in, int is_encrypted);

// Eight cipher clocks.
uint8_t clkByte(State *s, uint8_t in, int is_encrypted);

// Thirty-two cipher clocks with bit-reversed input (mfoc convention).
uint32_t clkWord(State *s, uint32_t in, int is_encrypted);

// 20-bit non-linear filter f(x) — usually only used by parity-encrypted
// auth-frame packing code. `static inline` so each TU gets its own copy.
static inline uint8_t filter(uint32_t x) {
    uint32_t f;
    f  = 0xf22c0u >> ( x        & 0xF) & 16u;
    f |= 0x6c9c0u >> ((x >>  4) & 0xF) &  8u;
    f |= 0x3c8b0u >> ((x >>  8) & 0xF) &  4u;
    f |= 0x1e458u >> ((x >> 12) & 0xF) &  2u;
    f |= 0x0d938u >> ((x >> 16) & 0xF) &  1u;
    return (uint8_t)((0xEC57E80Au >> f) & 1u);
}

// MIFARE tag PRNG (16-bit LFSR). Advances `x` by `n` steps.
uint32_t prng_successor(uint32_t x, uint32_t n);

}  // namespace niusCrypto1

#endif  // NIUS_CRYPTO1_H
