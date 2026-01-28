#ifndef PROTOCOL_POSEPACKET31_H
#define PROTOCOL_POSEPACKET31_H

#include <stdint.h>
#include "versioning.h"
#include "limits.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PosePacket31 - Binary motion packet (Brain → Muscle via RPMsg)
 *
 * Layout (42 bytes, little-endian):
 * Offset  Size  Field
 * ------  ----  -----
 *   0      2    magic (0xB31A)
 *   2      1    ver_major (3)
 *   3      1    ver_minor (1)
 *   4      4    seq (monotonic sequence number)
 *   8      4    t_ms (interpolation time to target)
 *  12      2    flags
 *  14     26    servo_us[13] (CH0-12, little-endian uint16)
 *  40      2    crc16 (over bytes 0..39)
 */

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;                       // 0xB31A
    uint8_t  ver_major;                   // 3
    uint8_t  ver_minor;                   // 1
    uint32_t seq;                         // monotonic counter
    uint32_t t_ms;                        // time to reach pose (ms)
    uint16_t flags;                       // bitfield
    uint16_t servo_us[SERVO_COUNT_TOTAL]; // 13 channels
    uint16_t crc16;                       // CRC-16/CCITT-FALSE
} PosePacket31;
#pragma pack(pop)

// Flags bitfield
#define FLAG_ESTOP        (1 << 0)  // bit0: Emergency stop
#define FLAG_HOLD         (1 << 1)  // bit1: Hold position
#define FLAG_CLAMP_ENABLE (1 << 2)  // bit2: Clamp enabled (always set)
#define FLAG_INTERP_FLOAT (0 << 3)  // bit3-4: Float interpolation
#define FLAG_INTERP_Q16   (1 << 3)  // bit3-4: Fixed Q16 interpolation
#define FLAG_SCAN_ENABLE  (1 << 5)  // bit5: Scan servo active

// Compile-time size check
#ifdef __cplusplus
static_assert(sizeof(PosePacket31) == 42, "PosePacket31 must be 42 bytes");
#else
_Static_assert(sizeof(PosePacket31) == 42, "PosePacket31 must be 42 bytes");
#endif

// Helper to initialize a neutral packet
static inline void posepacket31_init(PosePacket31 *pkt, uint32_t seq) {
    pkt->magic = SPIDER_MAGIC;
    pkt->ver_major = SPIDER_VERSION_MAJOR;
    pkt->ver_minor = SPIDER_VERSION_MINOR;
    pkt->seq = seq;
    pkt->t_ms = 0;
    pkt->flags = FLAG_CLAMP_ENABLE;
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt->servo_us[i] = SERVO_PWM_NEUTRAL_US;
    }
    pkt->crc16 = 0;
}

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_POSEPACKET31_H
