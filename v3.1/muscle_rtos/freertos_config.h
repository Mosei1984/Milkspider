/**
 * @file freertos_config.h
 * @brief FreeRTOS Task Configuration for Spider Robot v3.1 Muscle Runtime
 * 
 * Platform: Milk-V Duo 256M RISC-V Small Core (C906L @ 700MHz)
 * 
 * Timing Requirements:
 * - Motion loop: 50 Hz (20 ms period)
 * - Watchdog: 250 ms heartbeat timeout detection
 * - RPMsg: ISR context callback -> queue handoff
 */

#ifndef FREERTOS_CONFIG_TASKS_H
#define FREERTOS_CONFIG_TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*===========================================================================
 * TICK RATE CONFIGURATION
 *===========================================================================*/

/**
 * configTICK_RATE_HZ = 1000 Hz (1 ms tick)
 * 
 * Rationale:
 * - 20 ms motion period requires at least 50 Hz tick (20 ms granularity)
 * - 1 ms tick allows precise vTaskDelayUntil for 50 Hz motion
 * - Watchdog 250 ms timeout needs 4+ samples for debounce
 * - 1 kHz tick overhead acceptable on 700 MHz core (~0.1% CPU)
 */
#define SPIDER_TICK_RATE_HZ         1000

/*===========================================================================
 * TASK PRIORITY DEFINITIONS
 * Higher number = Higher priority (FreeRTOS convention)
 * configMAX_PRIORITIES should be >= 6
 *===========================================================================*/

#define TASK_PRIORITY_IDLE          0   /* FreeRTOS idle task */
#define TASK_PRIORITY_STATUS        1   /* Status reporting (lowest app task) */
#define TASK_PRIORITY_WATCHDOG      2   /* Heartbeat timeout detection */
#define TASK_PRIORITY_RPMSG_RX      3   /* RPMsg receive processing */
#define TASK_PRIORITY_MOTION        4   /* Real-time motion execution (highest) */

/* Safety: Motion must be highest app priority for determinism */
#define TASK_PRIORITY_MAX_APP       TASK_PRIORITY_MOTION

/*===========================================================================
 * STACK SIZE DEFINITIONS (in words, not bytes)
 * RISC-V 32-bit: 1 word = 4 bytes
 *===========================================================================*/

/**
 * Motion Task Stack: 512 words = 2048 bytes
 * - Interpolation calculations (float or q16)
 * - 13 servo target array (local)
 * - PCA9685 I2C buffer
 * - ISR preemption headroom
 */
#define STACK_SIZE_MOTION           512

/**
 * RPMsg RX Task Stack: 384 words = 1536 bytes
 * - PosePacket31 buffer (44 bytes)
 * - CRC calculation
 * - Queue copy overhead
 */
#define STACK_SIZE_RPMSG_RX         384

/**
 * Watchdog Task Stack: 256 words = 1024 bytes
 * - Simple timeout check
 * - State transition logic
 * - Minimal locals
 */
#define STACK_SIZE_WATCHDOG         256

/**
 * Status Task Stack: 256 words = 1024 bytes
 * - Telemetry struct assembly
 * - RPMsg status reply (optional)
 */
#define STACK_SIZE_STATUS           256

/*===========================================================================
 * QUEUE SIZE DEFINITIONS
 *===========================================================================*/

/**
 * Motion Command Queue: 2 items
 * - Current pose + next pose (double buffer)
 * - Brain sends at ~20-50 Hz, motion consumes at 50 Hz
 * - Queue=2 prevents blocking while allowing 1-frame lookahead
 */
#define QUEUE_SIZE_MOTION_CMD       2
#define QUEUE_ITEM_SIZE_MOTION_CMD  sizeof(PosePacket31)  /* 44 bytes */

/**
 * Status Report Queue: 4 items
 * - Status task runs lower priority, may lag
 * - Buffer up to 4 status updates
 */
#define QUEUE_SIZE_STATUS           4
#define QUEUE_ITEM_SIZE_STATUS      32  /* Status struct size */

/*===========================================================================
 * TIMING CONSTANTS
 *===========================================================================*/

/* Motion loop period: 20 ms = 50 Hz */
#define MOTION_PERIOD_MS            20
#define MOTION_PERIOD_TICKS         pdMS_TO_TICKS(MOTION_PERIOD_MS)

/* Watchdog check period: 50 ms (5 checks within 250 ms timeout) */
#define WATCHDOG_PERIOD_MS          50
#define WATCHDOG_PERIOD_TICKS       pdMS_TO_TICKS(WATCHDOG_PERIOD_MS)

/* Heartbeat timeout: 250 ms (from PROTOCOL.md) */
#define HEARTBEAT_TIMEOUT_MS        250
#define HEARTBEAT_TIMEOUT_TICKS     pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS)

/* Status report period: 200 ms (5 Hz telemetry) */
#define STATUS_PERIOD_MS            200
#define STATUS_PERIOD_TICKS         pdMS_TO_TICKS(STATUS_PERIOD_MS)

/*===========================================================================
 * I2C / PCA9685 TIMING
 *===========================================================================*/

/* PCA9685 I2C write: 13 servos × 4 registers × 9 bits @ 400 kHz ≈ 1.2 ms */
#define PCA9685_WRITE_TIME_US       1500  /* Conservative: 1.5 ms */

/* I2C bus mutex timeout */
#define I2C_MUTEX_TIMEOUT_MS        5
#define I2C_MUTEX_TIMEOUT_TICKS     pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)

/*===========================================================================
 * WORST CASE EXECUTION TIME (WCET) BUDGET
 * See TIMING_BUDGET.md for detailed analysis
 *===========================================================================*/

/* Per-task WCET in microseconds */
#define WCET_MOTION_US              3500  /* 3.5 ms (incl I2C) */
#define WCET_RPMSG_RX_US            200   /* 0.2 ms (CRC + queue) */
#define WCET_WATCHDOG_US            50    /* 0.05 ms */
#define WCET_STATUS_US              300   /* 0.3 ms */

/* Total CPU utilization budget: ~20% of 20 ms period */
#define CPU_UTILIZATION_TARGET_PCT  20

/*===========================================================================
 * SAFETY LIMITS
 *===========================================================================*/

/* Servo pulse width hard limits (from PROTOCOL.md) */
#define SERVO_US_MIN                500
#define SERVO_US_MAX                2500
#define SERVO_US_NEUTRAL            1500

/* Consecutive CRC failures before fault escalation */
#define CRC_FAIL_THRESHOLD          5

/* Consecutive watchdog timeouts before ESTOP */
#define WATCHDOG_FAIL_THRESHOLD     2

/*===========================================================================
 * TASK HANDLE DECLARATIONS (extern)
 *===========================================================================*/

extern TaskHandle_t xMotionTaskHandle;
extern TaskHandle_t xRpmsgRxTaskHandle;
extern TaskHandle_t xWatchdogTaskHandle;
extern TaskHandle_t xStatusTaskHandle;

extern QueueHandle_t xMotionCmdQueue;
extern QueueHandle_t xStatusQueue;

/*===========================================================================
 * TASK FUNCTION PROTOTYPES
 *===========================================================================*/

void vMotionTask(void *pvParameters);
void vRpmsgRxTask(void *pvParameters);
void vWatchdogTask(void *pvParameters);
void vStatusTask(void *pvParameters);

/* ISR callback from RPMsg (called in ISR context) */
void vRpmsgRxCallback(void *data, size_t len);

/*===========================================================================
 * INITIALIZATION
 *===========================================================================*/

/**
 * @brief Create all muscle runtime tasks and queues
 * @return pdPASS on success, pdFAIL on resource allocation failure
 */
BaseType_t xMuscleRtosInit(void);

#endif /* FREERTOS_CONFIG_TASKS_H */
