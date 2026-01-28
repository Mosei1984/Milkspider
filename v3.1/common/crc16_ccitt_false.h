#ifndef CRC16_CCITT_FALSE_H
#define CRC16_CCITT_FALSE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CRC-16/CCITT-FALSE
 * - Polynomial: 0x1021
 * - Initial:    0xFFFF
 * - RefIn:      false
 * - RefOut:     false
 * - XorOut:     0x0000
 *
 * Test vector: All servos 1500µs, seq=1, t_ms=100, flags=0x0004 → CRC = 0x29A4
 */
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CRC16_CCITT_FALSE_H
