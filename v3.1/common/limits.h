#ifndef SPIDER_LIMITS_H
#define SPIDER_LIMITS_H

#include <stdint.h>

// Servo PWM hard clamps (Muscle enforces these ALWAYS)
#define SERVO_PWM_MIN_US      500
#define SERVO_PWM_MAX_US      2500
#define SERVO_PWM_NEUTRAL_US  1500

// Servo angle soft limits (tightened from 20-160° per SAFETY_AUDIT.md §6.1)
// Provides ~35 µs margin to PWM clamps, preventing mechanical over-travel
#define SERVO_ANGLE_MIN_DEG   25
#define SERVO_ANGLE_MAX_DEG   155
#define SERVO_ANGLE_CENTER    90

// Channel allocation
#define SERVO_COUNT_LEGS      8
#define SERVO_COUNT_TOTAL     13
#define SERVO_CHANNEL_SCAN    12

// Timing limits
#define HEARTBEAT_TIMEOUT_MS  250
#define MOTION_UPDATE_HZ      50
#define EYE_RENDER_FPS        30

// Interpolation
#define INTERP_SUBSTEPS_MIN   1
#define INTERP_SUBSTEPS_MAX   16

// Stride scaling
#define STRIDE_FACTOR_MIN     0.3f
#define STRIDE_FACTOR_MAX     2.0f

// Calibration offset range
#define CALIB_OFFSET_MIN_DEG  -30
#define CALIB_OFFSET_MAX_DEG  30

// Inline clamp function
static inline uint16_t clamp_servo_us(uint16_t value) {
    if (value < SERVO_PWM_MIN_US) return SERVO_PWM_MIN_US;
    if (value > SERVO_PWM_MAX_US) return SERVO_PWM_MAX_US;
    return value;
}

#endif // SPIDER_LIMITS_H
