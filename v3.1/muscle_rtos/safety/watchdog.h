#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Watchdog subsystem for Spider Robot v3.1
 *
 * Monitors heartbeat from Brain (PosePacket31 reception).
 * Runs as independent FreeRTOS task with high priority.
 * Timeout: 250 ms → enters HOLD state.
 * ESTOP: immediate safe state.
 */

typedef enum {
    WATCHDOG_STATE_NORMAL,   // Receiving packets normally
    WATCHDOG_STATE_TIMEOUT,  // Heartbeat timeout detected
    WATCHDOG_STATE_HOLD,     // In hold state due to timeout
    WATCHDOG_STATE_ESTOP     // Emergency stop active
} WatchdogState;

typedef void (*WatchdogTimeoutCallback)(void);
typedef void (*WatchdogEstopCallback)(void);

/**
 * Initialize watchdog subsystem.
 * Must be called before watchdog_task_create().
 *
 * @param timeout_cb  Called on heartbeat timeout (from watchdog task context)
 * @param estop_cb    Called on ESTOP flag (from caller context)
 */
void watchdog_init(WatchdogTimeoutCallback timeout_cb, WatchdogEstopCallback estop_cb);

/**
 * Create and start the watchdog task.
 * @return 0 on success, -1 on failure
 */
int watchdog_task_create(void);

/**
 * Feed the watchdog (call when valid PosePacket31 received).
 * Thread-safe, can be called from any task.
 */
void watchdog_feed(void);

/**
 * Signal ESTOP condition.
 * Immediately triggers ESTOP callback and enters ESTOP state.
 * Thread-safe.
 */
void watchdog_signal_estop(void);

/**
 * Clear ESTOP state (requires external command).
 * Returns to NORMAL state only if heartbeat is active.
 * @return 0 on success, -1 if conditions not met
 */
int watchdog_clear_estop(void);

/**
 * Get current watchdog state.
 * Thread-safe (atomic read).
 */
WatchdogState watchdog_get_state(void);

/**
 * Check if motion is allowed.
 * @return true if in NORMAL state and not timed out
 */
bool watchdog_is_motion_allowed(void);

/**
 * Get milliseconds since last feed.
 */
uint32_t watchdog_get_ms_since_feed(void);

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_H
