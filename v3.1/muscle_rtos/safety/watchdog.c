/**
 * Watchdog Subsystem Implementation
 *
 * Independent FreeRTOS task monitors heartbeat from Brain.
 * - 250 ms timeout → HOLD state
 * - ESTOP flag → immediate safe state
 * - Uses atomic operations for thread-safe state access
 */

#include "watchdog.h"
#include "failsafe.h"
#include "fault_flags.h"

#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#include "limits.h"

#include <stdatomic.h>

#define WATCHDOG_TASK_STACK     256
#define WATCHDOG_TASK_PRIORITY  (configMAX_PRIORITIES - 1)  // Highest priority
#define WATCHDOG_CHECK_PERIOD_MS  25  // Check 4x per timeout period

static TaskHandle_t s_watchdog_task_handle = NULL;

static volatile _Atomic WatchdogState s_state = WATCHDOG_STATE_NORMAL;
static volatile _Atomic TickType_t s_last_feed_tick = 0;

static WatchdogTimeoutCallback s_timeout_cb = NULL;
static WatchdogEstopCallback s_estop_cb = NULL;

static void watchdog_task_entry(void *pvParameters);

void watchdog_init(WatchdogTimeoutCallback timeout_cb, WatchdogEstopCallback estop_cb) {
    s_timeout_cb = timeout_cb;
    s_estop_cb = estop_cb;
    s_last_feed_tick = xTaskGetTickCount();
    atomic_store(&s_state, WATCHDOG_STATE_NORMAL);
}

int watchdog_task_create(void) {
    BaseType_t result = xTaskCreate(
        watchdog_task_entry,
        "watchdog",
        WATCHDOG_TASK_STACK,
        NULL,
        WATCHDOG_TASK_PRIORITY,
        &s_watchdog_task_handle
    );

    return (result == pdPASS) ? 0 : -1;
}

void watchdog_feed(void) {
    atomic_store(&s_last_feed_tick, xTaskGetTickCount());

    WatchdogState current = atomic_load(&s_state);
    if (current == WATCHDOG_STATE_TIMEOUT || current == WATCHDOG_STATE_HOLD) {
        atomic_store(&s_state, WATCHDOG_STATE_NORMAL);
        fault_flags_clear(FAULT_HEARTBEAT_TIMEOUT);
    }
}

void watchdog_signal_estop(void) {
    atomic_store(&s_state, WATCHDOG_STATE_ESTOP);
    fault_flags_set(FAULT_ESTOP_ACTIVE);

    if (s_estop_cb != NULL) {
        s_estop_cb();
    }
}

int watchdog_clear_estop(void) {
    WatchdogState current = atomic_load(&s_state);
    if (current != WATCHDOG_STATE_ESTOP) {
        return -1;
    }

    TickType_t now = xTaskGetTickCount();
    TickType_t last = atomic_load(&s_last_feed_tick);

    if ((now - last) < pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS)) {
        atomic_store(&s_state, WATCHDOG_STATE_NORMAL);
        fault_flags_clear(FAULT_ESTOP_ACTIVE);
        return 0;
    }

    atomic_store(&s_state, WATCHDOG_STATE_HOLD);
    return -1;
}

WatchdogState watchdog_get_state(void) {
    return atomic_load(&s_state);
}

bool watchdog_is_motion_allowed(void) {
    WatchdogState current = atomic_load(&s_state);
    return (current == WATCHDOG_STATE_NORMAL);
}

uint32_t watchdog_get_ms_since_feed(void) {
    TickType_t now = xTaskGetTickCount();
    TickType_t last = atomic_load(&s_last_feed_tick);
    return (uint32_t)((now - last) * portTICK_PERIOD_MS);
}

static void watchdog_task_entry(void *pvParameters) {
    (void)pvParameters;

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        TickType_t now = xTaskGetTickCount();
        TickType_t last_feed = atomic_load(&s_last_feed_tick);
        WatchdogState current = atomic_load(&s_state);

        if (current == WATCHDOG_STATE_ESTOP) {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(WATCHDOG_CHECK_PERIOD_MS));
            continue;
        }

        TickType_t elapsed = now - last_feed;

        if (elapsed > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS)) {
            if (current == WATCHDOG_STATE_NORMAL) {
                atomic_store(&s_state, WATCHDOG_STATE_TIMEOUT);
                fault_flags_set(FAULT_HEARTBEAT_TIMEOUT);

                if (s_timeout_cb != NULL) {
                    s_timeout_cb();
                }
            }

            if (atomic_load(&s_state) == WATCHDOG_STATE_TIMEOUT) {
                atomic_store(&s_state, WATCHDOG_STATE_HOLD);
                failsafe_enter_hold();
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(WATCHDOG_CHECK_PERIOD_MS));
    }
}
