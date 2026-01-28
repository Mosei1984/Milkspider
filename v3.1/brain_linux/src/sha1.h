/**
 * Minimal SHA-1 implementation for WebSocket handshake
 * Public domain / CC0
 */

#ifndef SHA1_MINIMAL_H
#define SHA1_MINIMAL_H

#include <cstdint>
#include <cstddef>
#include <cstring>

static inline uint32_t sha1_rol(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

static inline void sha1(const uint8_t* data, size_t len, uint8_t hash[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t* msg = new uint8_t[padded_len]();
    memcpy(msg, data, len);
    msg[len] = 0x80;

    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; i++) {
        msg[padded_len - 1 - i] = (bit_len >> (i * 8)) & 0xFF;
    }

    for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (msg[chunk + i*4] << 24) | (msg[chunk + i*4 + 1] << 16) |
                   (msg[chunk + i*4 + 2] << 8) | msg[chunk + i*4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = sha1_rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;            k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;            k = 0xCA62C1D6; }

            uint32_t temp = sha1_rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = sha1_rol(b, 30); b = a; a = temp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    delete[] msg;

    uint32_t hh[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        hash[i*4]     = (hh[i] >> 24) & 0xFF;
        hash[i*4 + 1] = (hh[i] >> 16) & 0xFF;
        hash[i*4 + 2] = (hh[i] >> 8) & 0xFF;
        hash[i*4 + 3] = hh[i] & 0xFF;
    }
}

#endif // SHA1_MINIMAL_H
