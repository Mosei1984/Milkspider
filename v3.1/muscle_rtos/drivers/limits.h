/**
 * Spider Robot v3.1 - Safety Limits (FreeRTOS Muscle Runtime)
 * 
 * NOTE: Keep in sync with common/limits.h
 */

#ifndef SPIDER_LIMITS_H
#define SPIDER_LIMITS_H

#include <stdint.h>

// Servo PWM hard clamps (ALWAYS enforced)
#define SERVO_PWM_MIN_US      500
#define SERVO_PWM_MAX_US      2500
#define SERVO_PWM_NEUTRAL_US  1500

// Channel allocation
#define SERVO_COUNT_LEGS      8
#define SERVO_COUNT_TOTAL     13
#define SERVO_CHANNEL_SCAN    12

// Timing and safety
#define HEARTBEAT_TIMEOUT_MS  250   // Watchdog timeout (Brain must send within this)
#define MOTION_UPDATE_HZ      50

// Inline clamp function
static inline uint16_t clamp_servo_us(uint16_t value) {
    if (value < SERVO_PWM_MIN_US) return SERVO_PWM_MIN_US;
    if (value > SERVO_PWM_MAX_US) return SERVO_PWM_MAX_US;
    return value;
}

#endif // SPIDER_LIMITS_H
