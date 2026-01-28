#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "limits.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INTERP_MODE_FLOAT,
    INTERP_MODE_Q16
} InterpMode;

/**
 * Start interpolation from current to target over duration_ms.
 */
void interpolator_start(const uint16_t *current_us, const uint16_t *target_us,
                        uint32_t duration_ms, InterpMode mode);

/**
 * Tick the interpolator (call at 50 Hz).
 * Updates output_us array with interpolated values.
 * Returns true when interpolation is complete.
 */
bool interpolator_tick(uint16_t *output_us);

/**
 * Abort current interpolation (freeze at current position).
 */
void interpolator_abort(void);

#ifdef __cplusplus
}
#endif

#endif // INTERPOLATOR_H
