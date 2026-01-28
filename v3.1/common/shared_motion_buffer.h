/**
 * Shared Memory Ring Buffer for Spider Robot v3.1 Inter-Core Communication
 * 
 * Used by BOTH Linux (Brain) and FreeRTOS (Muscle) for zero-copy packet transfer.
 * 
 * Memory Layout at 0x83F00000 (256KB reserved):
 * ┌────────────────────────────────────────┐
 * │ SharedMotionBuffer (528 bytes)         │
 * │ ├─ write_idx (4)   - Linux writes      │
 * │ ├─ read_idx (4)    - FreeRTOS writes   │
 * │ ├─ flags (4)       - Status flags      │
 * │ ├─ reserved (4)    - Alignment         │
 * │ └─ packets[8][64]  - Ring buffer slots │
 * └────────────────────────────────────────┘
 * 
 * Flow:
 * 1. Linux writes PosePacket31 to packets[write_idx % 8]
 * 2. Linux increments write_idx (memory barrier)
 * 3. Linux sends mailbox notification via /dev/cvi-rtos-cmdqu
 * 4. FreeRTOS receives mailbox IRQ
 * 5. FreeRTOS reads packets from read_idx to write_idx
 * 6. FreeRTOS increments read_idx after processing
 */

#ifndef SHARED_MOTION_BUFFER_H
#define SHARED_MOTION_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHARED_MEM_BASE         0x83F00000
#define SHARED_MEM_SIZE         0x40000     // 256KB reserved for FreeRTOS comm

#define PACKET_RING_SIZE        8           // Must be power of 2
#define PACKET_SLOT_SIZE        64          // Each slot fits PosePacket31 (42 bytes) + padding

#define SHARED_FLAG_BRAIN_READY     (1U << 0)
#define SHARED_FLAG_MUSCLE_READY    (1U << 1)
#define SHARED_FLAG_ESTOP           (1U << 2)
#define SHARED_FLAG_OVERFLOW        (1U << 3)

#define CMD_MOTION_PACKET       0x20
#define CMD_MOTION_ACK          0x21
#define CMD_MOTION_NACK         0x22
#define CMD_MOTION_HEARTBEAT    0x23

#pragma pack(push, 1)
typedef struct {
    volatile uint32_t write_idx;    // Linux writes, FreeRTOS reads (monotonic counter)
    volatile uint32_t read_idx;     // FreeRTOS writes, Linux reads (monotonic counter)
    volatile uint32_t flags;        // Status flags (see SHARED_FLAG_*)
    volatile uint32_t reserved;     // Alignment padding / future use
    volatile uint8_t  packets[PACKET_RING_SIZE][PACKET_SLOT_SIZE];
} SharedMotionBuffer;
#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(SharedMotionBuffer) == 528, "SharedMotionBuffer must be 528 bytes");
#else
_Static_assert(sizeof(SharedMotionBuffer) == 528, "SharedMotionBuffer must be 528 bytes");
#endif

static inline uint32_t shared_buffer_available(const volatile SharedMotionBuffer *buf) {
    return buf->write_idx - buf->read_idx;
}

static inline int shared_buffer_is_full(const volatile SharedMotionBuffer *buf) {
    return (buf->write_idx - buf->read_idx) >= PACKET_RING_SIZE;
}

static inline int shared_buffer_is_empty(const volatile SharedMotionBuffer *buf) {
    return buf->write_idx == buf->read_idx;
}

static inline uint32_t shared_buffer_slot_index(uint32_t idx) {
    return idx % PACKET_RING_SIZE;  // Works because PACKET_RING_SIZE is power of 2
}

#ifdef __cplusplus
}
#endif

#endif // SHARED_MOTION_BUFFER_H
