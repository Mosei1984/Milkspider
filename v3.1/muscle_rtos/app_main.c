/**
 * Spider Robot v3.1 - Muscle Runtime (FreeRTOS)
 *
 * This is the real-time core running on the RISC-V small core.
 * Responsibilities:
 * - Receive PosePacket31 via RPMsg from Brain
 * - Validate CRC and clamp servo values
 * - Interpolate motion and output to PCA9685
 * - Enforce safety (watchdog, ESTOP, hold)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "versioning.h"
#include "limits.h"
#include "protocol_posepacket31.h"
#include "crc16_ccitt_false.h"

#include "rpmsg_motion_rx.h"
#include "motion_task.h"
#include "pca9685.h"
#include "failsafe.h"
#include "fault_flags.h"
#include "watchdog.h"

// Task handles
static TaskHandle_t motion_task_handle = NULL;

// Watchdog timeout callback - called from watchdog task
static void on_watchdog_timeout(void) {
    // Watchdog handles HOLD via failsafe_enter_hold()
}

// ESTOP callback - immediate safe state
static void on_watchdog_estop(void) {
    failsafe_enter_estop();
}

// Motion packet queue (Brain → Motion Task)
QueueHandle_t g_motion_queue = NULL;

#define MOTION_QUEUE_LENGTH  4
#define MOTION_TASK_STACK    512
#define MOTION_TASK_PRIORITY 3
#define WATCHDOG_ENABLED     1

void app_main(void) {
    // Initialize fault flags
    fault_flags_init();

    // Initialize PCA9685 servo driver
    if (pca9685_init(PCA9685_I2C_ADDR_DEFAULT) != 0) {
        fault_flags_set(FAULT_PCA9685_INIT);
    }

    // Set all servos to neutral
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pca9685_set_pwm_us(i, SERVO_PWM_NEUTRAL_US);
    }

    // Create motion packet queue
    g_motion_queue = xQueueCreate(MOTION_QUEUE_LENGTH, sizeof(PosePacket31));
    if (g_motion_queue == NULL) {
        fault_flags_set(FAULT_QUEUE_CREATE);
    }

    // Initialize RPMsg receiver (registers callback)
    rpmsg_motion_rx_init(g_motion_queue);

    // Create motion processing task
    xTaskCreate(
        motion_task_entry,
        "motion",
        MOTION_TASK_STACK,
        NULL,
        MOTION_TASK_PRIORITY,
        &motion_task_handle
    );

#if WATCHDOG_ENABLED
    // Initialize and create watchdog task (high priority, independent)
    watchdog_init(on_watchdog_timeout, on_watchdog_estop);
    if (watchdog_task_create() != 0) {
        fault_flags_set(FAULT_QUEUE_CREATE);
    }
#endif

    // FreeRTOS scheduler started by platform boot code
}
