/**
 * Spider Robot v3.1 - Muscle Runtime (FreeRTOS)
 * SDK v1 Compatible Version
 * 
 * Runs on the RISC-V small core (C906 @ 700MHz)
 * Receives mailbox commands from Linux Brain and drives servos via PCA9685.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "pca9685.h"
#include "limits.h"
#include "versioning.h"
#include "shared_motion_buffer.h"
#include "protocol_posepacket31.h"
#include "crc16_ccitt_false.h"
#include "safety/fault_flags.h"
#include "safety/watchdog.h"
#include "safety/failsafe.h"

#define MOTION_QUEUE_LENGTH   4
#define MOTION_TASK_STACK     512
#define MOTION_TASK_PRIORITY  4

#define CMD_HEARTBEAT         0x10
#define CMD_MOTION_PACKET     0x20
#define CMD_ESTOP             0x30

static QueueHandle_t g_motion_queue = NULL;
static volatile SharedMotionBuffer *g_shared_buf = NULL;
static volatile uint32_t g_last_seq = 0;
static volatile uint32_t g_rx_count = 0;
static volatile uint32_t g_drop_count = 0;
static volatile int g_estop_active = 0;

// Forward declarations
static void set_all_servos_neutral(void);

// Watchdog callbacks
static void on_watchdog_timeout(void) {
    printf("[Spider] WATCHDOG: Heartbeat timeout!\n");
    set_all_servos_neutral();
}

static void on_watchdog_estop(void) {
    printf("[Spider] WATCHDOG: ESTOP triggered!\n");
    failsafe_enter_estop();
}

static uint16_t compute_crc16(const uint8_t *data, size_t len) {
    return crc16_ccitt_false(data, len);
}

static int validate_packet(const PosePacket31 *pkt) {
    if (pkt->magic != SPIDER_MAGIC) {
        fault_flags_set(FAULT_PACKET_MAGIC);
        g_drop_count++;
        printf("[Spider] Bad magic: 0x%04X\n", pkt->magic);
        return -1;
    }

    if (pkt->ver_major != SPIDER_VERSION_MAJOR || 
        pkt->ver_minor != SPIDER_VERSION_MINOR) {
        fault_flags_set(FAULT_PACKET_VERSION);
        g_drop_count++;
        printf("[Spider] Bad version: %d.%d\n", pkt->ver_major, pkt->ver_minor);
        return -2;
    }

    uint16_t computed_crc = compute_crc16((const uint8_t *)pkt, 
                                           sizeof(PosePacket31) - 2);
    if (computed_crc != pkt->crc16) {
        fault_flags_set(FAULT_PACKET_CRC);
        g_drop_count++;
        printf("[Spider] CRC mismatch: calc=0x%04X pkt=0x%04X\n", computed_crc, pkt->crc16);
        return -3;
    }

    if (pkt->seq <= g_last_seq && g_last_seq != 0) {
        g_drop_count++;
        return -4;
    }

    return 0;
}

static void apply_servo_values(const PosePacket31 *pkt) {
    for (int ch = 0; ch < SERVO_COUNT_TOTAL && ch < 13; ch++) {
        uint16_t pulse_us = pkt->servo_us[ch];
        pca9685_set_pwm_us(ch, pulse_us);
    }
}

static void set_all_servos_neutral(void) {
    pca9685_set_all_us(SERVO_PWM_NEUTRAL_US);
}

static void handle_estop(void) {
    g_estop_active = 1;
    fault_flags_set(FAULT_ESTOP_ACTIVE);
    
    set_all_servos_neutral();
    
    if (g_shared_buf != NULL) {
        g_shared_buf->flags |= SHARED_FLAG_ESTOP;
        __asm volatile ("fence rw, rw" ::: "memory");
    }
    
    printf("[Spider] ESTOP activated - all servos neutral\n");
}

static void handle_heartbeat(void) {
    watchdog_feed();  // Feed watchdog on heartbeat
}

static int process_shared_buffer_packets(void) {
    if (g_shared_buf == NULL) {
        return 0;
    }

    __asm volatile ("fence rw, rw" ::: "memory");
    
    uint32_t read_idx = g_shared_buf->read_idx;
    uint32_t write_idx = g_shared_buf->write_idx;
    int processed = 0;
    
    while (read_idx != write_idx) {
        if (g_estop_active) {
            read_idx++;
            g_shared_buf->read_idx = read_idx;
            __asm volatile ("fence rw, rw" ::: "memory");
            continue;
        }

        uint32_t slot = shared_buffer_slot_index(read_idx);
        const PosePacket31 *pkt = 
            (const PosePacket31 *)g_shared_buf->packets[slot];
        
        if (validate_packet(pkt) == 0) {
            watchdog_feed();  // Feed watchdog on valid packet
            
            if (pkt->flags & FLAG_ESTOP) {
                handle_estop();
            } else {
                apply_servo_values(pkt);
                g_last_seq = pkt->seq;
                g_rx_count++;
                processed++;
            }
        }
        
        read_idx++;
        g_shared_buf->read_idx = read_idx;
        __asm volatile ("fence rw, rw" ::: "memory");
    }

    return processed;
}

void mailbox_cmd_handler(uint8_t cmd_id, uint32_t param) {
    (void)param;
    
    switch (cmd_id) {
    case CMD_HEARTBEAT:
        handle_heartbeat();
        break;
        
    case CMD_MOTION_PACKET:
        if (!g_estop_active) {
            int n = process_shared_buffer_packets();
            if (n > 0) {
                printf("[Spider] Processed %d motion packets\n", n);
            }
        }
        break;
        
    case CMD_ESTOP:
        handle_estop();
        break;
        
    default:
        printf("[Spider] Unknown cmd: 0x%02X\n", cmd_id);
        break;
    }
}

static void motion_task_entry(void *pvParameters) {
    (void)pvParameters;
    
    printf("[Spider] Motion task started\n");
    
    PosePacket31 pkt;
    TickType_t last_status_time = xTaskGetTickCount();
    
    while (1) {
        if (xQueueReceive(g_motion_queue, &pkt, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (g_estop_active) {
                continue;
            }
            
            if (validate_packet(&pkt) == 0) {
                if (pkt.flags & FLAG_ESTOP) {
                    handle_estop();
                } else {
                    apply_servo_values(&pkt);
                    g_last_seq = pkt.seq;
                    g_rx_count++;
                }
            }
        }
        
        if (!g_estop_active) {
            int n = process_shared_buffer_packets();
            (void)n;
        }
        
        TickType_t now = xTaskGetTickCount();
        if ((now - last_status_time) >= pdMS_TO_TICKS(5000)) {
            printf("[Spider] Status: rx=%lu drop=%lu seq=%lu estop=%d\n",
                   (unsigned long)g_rx_count, 
                   (unsigned long)g_drop_count,
                   (unsigned long)g_last_seq,
                   g_estop_active);
            last_status_time = now;
        }
    }
}

void main_cvirtos(void) {
    printf("[Spider] ================================\n");
    printf("[Spider] Muscle Runtime v3.1 Starting\n");
    printf("[Spider] ================================\n");
    
    fault_flags_init();
    
    if (pca9685_init(PCA9685_I2C_ADDR_DEFAULT) != 0) {
        printf("[Spider] ERROR: PCA9685 init failed!\n");
        fault_flags_set(FAULT_PCA9685_INIT);
    } else {
        printf("[Spider] PCA9685 initialized at 50Hz\n");
        set_all_servos_neutral();
        printf("[Spider] All servos set to neutral (%d us)\n", SERVO_PWM_NEUTRAL_US);
    }
    
    g_shared_buf = (volatile SharedMotionBuffer *)SHARED_MEM_BASE;
    g_shared_buf->read_idx = 0;
    g_shared_buf->flags |= SHARED_FLAG_MUSCLE_READY;
    __asm volatile ("fence rw, rw" ::: "memory");
    printf("[Spider] Shared memory at 0x%08lX ready\n", (unsigned long)SHARED_MEM_BASE);
    
    g_motion_queue = xQueueCreate(MOTION_QUEUE_LENGTH, sizeof(PosePacket31));
    if (g_motion_queue == NULL) {
        printf("[Spider] ERROR: Failed to create motion queue!\n");
        fault_flags_set(FAULT_QUEUE_CREATE);
        return;
    }
    printf("[Spider] Motion queue created\n");
    
    BaseType_t ret = xTaskCreate(
        motion_task_entry,
        "motion",
        MOTION_TASK_STACK,
        NULL,
        MOTION_TASK_PRIORITY,
        NULL
    );
    
    if (ret != pdPASS) {
        printf("[Spider] ERROR: Failed to create motion task!\n");
        fault_flags_set(FAULT_QUEUE_CREATE);
        return;
    }
    
    printf("[Spider] Motion task created\n");
    
    // Initialize and start watchdog
    watchdog_init(on_watchdog_timeout, on_watchdog_estop);
    if (watchdog_task_create() == 0) {
        printf("[Spider] Watchdog task created (timeout=%dms)\n", HEARTBEAT_TIMEOUT_MS);
    } else {
        printf("[Spider] WARNING: Watchdog task creation failed!\n");
    }
    
    printf("[Spider] Initialization complete!\n");
    printf("[Spider] Waiting for Brain commands...\n");
}
