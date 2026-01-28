#ifndef FAILSAFE_H
#define FAILSAFE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Failsafe module - handles ESTOP and safe states.
 */

/**
 * Enter ESTOP state.
 * - Immediately stops all motion
 * - Sets all servos to neutral (1500 µs)
 * - Sets ESTOP fault flag
 * - Blocks further motion until cleared
 */
void failsafe_enter_estop(void);

/**
 * Clear ESTOP state (requires explicit external command).
 * Returns 0 if cleared successfully.
 */
int failsafe_clear_estop(void);

/**
 * Check if ESTOP is active.
 */
int failsafe_is_estop_active(void);

/**
 * Enter HOLD state (maintain current position).
 */
void failsafe_enter_hold(void);

/**
 * Set safe pose (all servos to neutral).
 */
void failsafe_set_safe_pose(void);

#ifdef __cplusplus
}
#endif

#endif // FAILSAFE_H
