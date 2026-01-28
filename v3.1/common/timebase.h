#ifndef SPIDER_TIMEBASE_H
#define SPIDER_TIMEBASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform-agnostic time interface.
 * Implementations differ for Linux (clock_gettime) and FreeRTOS (xTaskGetTickCount).
 */

// Get current time in milliseconds (monotonic)
uint32_t timebase_millis(void);

// Get current time in microseconds (monotonic, if available)
uint64_t timebase_micros(void);

// Sleep for specified milliseconds (non-blocking on RTOS, blocking on Linux)
void timebase_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // SPIDER_TIMEBASE_H
