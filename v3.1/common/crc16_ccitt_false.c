#include "crc16_ccitt_false.h"

#define CRC16_POLY 0x1021
#define CRC16_INIT 0xFFFF

uint16_t crc16_ccitt_false(const uint8_t *data, size_t len) {
    uint16_t crc = CRC16_INIT;

    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
