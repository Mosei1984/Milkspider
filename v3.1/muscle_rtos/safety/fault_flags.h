#ifndef FAULT_FLAGS_H
#define FAULT_FLAGS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fault flags for diagnostics and safety monitoring.
 * These are set by various subsystems and can be queried/cleared.
 */

typedef enum {
    FAULT_NONE              = 0,
    FAULT_PCA9685_INIT      = (1 << 0),
    FAULT_QUEUE_CREATE      = (1 << 1),
    FAULT_RPMSG_SIZE        = (1 << 2),
    FAULT_PACKET_MAGIC      = (1 << 3),
    FAULT_PACKET_VERSION    = (1 << 4),
    FAULT_PACKET_CRC        = (1 << 5),
    FAULT_HEARTBEAT_TIMEOUT = (1 << 6),
    FAULT_SERVO_CLAMP       = (1 << 7),  // Value was clamped (info, not error)
    FAULT_I2C_ERROR         = (1 << 8),
    FAULT_ESTOP_ACTIVE      = (1 << 9),
} FaultFlag;

/**
 * Initialize fault flags system.
 */
void fault_flags_init(void);

/**
 * Set a fault flag (atomic).
 */
void fault_flags_set(FaultFlag flag);

/**
 * Clear a fault flag.
 */
void fault_flags_clear(FaultFlag flag);

/**
 * Check if a specific fault is set.
 */
bool fault_flags_is_set(FaultFlag flag);

/**
 * Get all active fault flags.
 */
uint32_t fault_flags_get_all(void);

/**
 * Clear all fault flags.
 */
void fault_flags_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif // FAULT_FLAGS_H
