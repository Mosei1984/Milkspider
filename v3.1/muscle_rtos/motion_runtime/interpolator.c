/**
 * Motion Interpolator
 *
 * Smoothly interpolates servo positions from start to target.
 * Supports float and Q16.16 fixed-point modes.
 */

#include "interpolator.h"
#include <string.h>

// Interpolation state
static uint16_t s_start_us[SERVO_COUNT_TOTAL];
static uint16_t s_target_us[SERVO_COUNT_TOTAL];
static uint32_t s_duration_ms;
static uint32_t s_elapsed_ms;
static InterpMode s_mode;
static bool s_active;

#define TICK_PERIOD_MS  (1000 / MOTION_UPDATE_HZ)  // 20 ms

void interpolator_start(const uint16_t *current_us, const uint16_t *target_us,
                        uint32_t duration_ms, InterpMode mode) {
    memcpy(s_start_us, current_us, sizeof(s_start_us));
    memcpy(s_target_us, target_us, sizeof(s_target_us));

    s_duration_ms = (duration_ms > 0) ? duration_ms : 1;
    s_elapsed_ms = 0;
    s_mode = mode;
    s_active = true;
}

bool interpolator_tick(uint16_t *output_us) {
    if (!s_active) {
        return true;  // Already complete
    }

    s_elapsed_ms += TICK_PERIOD_MS;

    if (s_elapsed_ms >= s_duration_ms) {
        // Interpolation complete - snap to target
        memcpy(output_us, s_target_us, sizeof(s_target_us));
        s_active = false;
        return true;
    }

    // Calculate interpolation factor
    if (s_mode == INTERP_MODE_FLOAT) {
        float t = (float)s_elapsed_ms / (float)s_duration_ms;
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            float start = (float)s_start_us[i];
            float target = (float)s_target_us[i];
            output_us[i] = (uint16_t)(start + (target - start) * t);
        }
    } else {
        // Q16.16 fixed-point
        uint32_t t_q16 = (s_elapsed_ms << 16) / s_duration_ms;
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            int32_t start = (int32_t)s_start_us[i];
            int32_t target = (int32_t)s_target_us[i];
            int32_t delta = target - start;
            int32_t interp = start + ((delta * (int32_t)t_q16) >> 16);
            output_us[i] = (uint16_t)interp;
        }
    }

    return false;
}

void interpolator_abort(void) {
    s_active = false;
}
