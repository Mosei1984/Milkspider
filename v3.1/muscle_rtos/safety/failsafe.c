/**
 * Failsafe Implementation
 */

#include "failsafe.h"
#include "fault_flags.h"
#include "pca9685.h"
#include "limits.h"

static volatile int s_estop_active = 0;

void failsafe_enter_estop(void) {
    s_estop_active = 1;
    fault_flags_set(FAULT_ESTOP_ACTIVE);

    // Immediately set all servos to neutral
    failsafe_set_safe_pose();
}

int failsafe_clear_estop(void) {
    if (!s_estop_active) {
        return 0;  // Not in ESTOP
    }

    s_estop_active = 0;
    fault_flags_clear(FAULT_ESTOP_ACTIVE);
    return 0;
}

int failsafe_is_estop_active(void) {
    return s_estop_active;
}

void failsafe_enter_hold(void) {
    // HOLD maintains current position
    // Motion task handles this by not updating targets
}

void failsafe_set_safe_pose(void) {
    pca9685_set_all_us(SERVO_PWM_NEUTRAL_US);
}
