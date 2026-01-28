/**
 * Motion Task
 *
 * Core motion processing loop:
 * 1. Wait for PosePacket31 from queue
 * 2. Check ESTOP flag → immediate hold
 * 3. Clamp all servo values to 500-2500 µs
 * 4. Interpolate from current to target over t_ms
 * 5. Output to PCA9685 at 50 Hz
 * 6. Monitor watchdog (250 ms timeout)
 */

#include "motion_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "limits.h"
#include "protocol_posepacket31.h"
#include "interpolator.h"
#include "pca9685.h"
#include "failsafe.h"
#include "fault_flags.h"
#include "watchdog.h"

extern QueueHandle_t g_motion_queue;

// Current servo positions (µs)
static uint16_t s_current_us[SERVO_COUNT_TOTAL];

// Target servo positions (µs)
static uint16_t s_target_us[SERVO_COUNT_TOTAL];

// Last valid packet timestamp
static TickType_t s_last_packet_tick = 0;

// Motion state
typedef enum {
    MOTION_STATE_IDLE,
    MOTION_STATE_MOVING,
    MOTION_STATE_HOLD,
    MOTION_STATE_ESTOP
} MotionState;

static MotionState s_state = MOTION_STATE_IDLE;

#define MOTION_UPDATE_PERIOD_MS  (1000 / MOTION_UPDATE_HZ)  // 20 ms for 50 Hz

void motion_task_entry(void *pvParameters) {
    (void)pvParameters;

    // Initialize current positions to neutral
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        s_current_us[i] = SERVO_PWM_NEUTRAL_US;
        s_target_us[i] = SERVO_PWM_NEUTRAL_US;
    }

    s_last_packet_tick = xTaskGetTickCount();

    PosePacket31 pkt;
    TickType_t last_update = xTaskGetTickCount();

    while (1) {
        // Check for new packet (non-blocking with short timeout)
        if (xQueueReceive(g_motion_queue, &pkt, pdMS_TO_TICKS(MOTION_UPDATE_PERIOD_MS / 2)) == pdTRUE) {
            s_last_packet_tick = xTaskGetTickCount();

            // Feed the independent watchdog
            watchdog_feed();

            // ESTOP has absolute priority
            if (pkt.flags & FLAG_ESTOP) {
                s_state = MOTION_STATE_ESTOP;
                watchdog_signal_estop();
                continue;
            }

            // HOLD flag
            if (pkt.flags & FLAG_HOLD) {
                s_state = MOTION_STATE_HOLD;
                continue;
            }

            // Apply clamp to all servo targets (MANDATORY)
            for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
                s_target_us[i] = clamp_servo_us(pkt.servo_us[i]);
            }

            // Start interpolation
            interpolator_start(s_current_us, s_target_us, pkt.t_ms,
                               (pkt.flags & FLAG_INTERP_Q16) ? INTERP_MODE_Q16 : INTERP_MODE_FLOAT);

            s_state = MOTION_STATE_MOVING;
        }

        // Sync local state with watchdog (watchdog task handles timeout detection)
        WatchdogState wdog_state = watchdog_get_state();
        if (wdog_state == WATCHDOG_STATE_ESTOP && s_state != MOTION_STATE_ESTOP) {
            s_state = MOTION_STATE_ESTOP;
        } else if ((wdog_state == WATCHDOG_STATE_TIMEOUT || wdog_state == WATCHDOG_STATE_HOLD) 
                   && s_state != MOTION_STATE_ESTOP) {
            s_state = MOTION_STATE_HOLD;
        }

        // State machine
        switch (s_state) {
            case MOTION_STATE_MOVING:
                if (interpolator_tick(s_current_us)) {
                    // Interpolation complete
                    s_state = MOTION_STATE_IDLE;
                }
                break;

            case MOTION_STATE_HOLD:
            case MOTION_STATE_IDLE:
                // Hold current position (no change)
                break;

            case MOTION_STATE_ESTOP:
                // ESTOP: stay in safe state until cleared externally
                break;
        }

        // Output to servos (always, even in HOLD - maintains position)
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            pca9685_set_pwm_us(i, s_current_us[i]);
        }

        // Maintain 50 Hz update rate
        vTaskDelayUntil(&last_update, pdMS_TO_TICKS(MOTION_UPDATE_PERIOD_MS));
    }
}
