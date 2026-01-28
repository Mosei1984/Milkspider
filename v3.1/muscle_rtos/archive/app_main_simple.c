/**
 * Spider Robot v3.1 - Muscle Runtime (FreeRTOS)
 * Simple Version - Core functionality without I2C/PCA9685
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Motion queue for receiving PosePackets from Brain */
static QueueHandle_t g_motion_queue = NULL;

#define MOTION_QUEUE_LENGTH  4
#define MOTION_TASK_STACK    256
#define MOTION_TASK_PRIORITY 3

/* Simple motion task - receives packets and logs them */
static void motion_task_entry(void *pvParameters) {
    (void)pvParameters;
    
    printf("[Spider] Motion task started\n");
    
    uint8_t packet[64];
    uint32_t packet_count = 0;
    
    while (1) {
        /* Wait for packet from Brain (via mailbox) */
        if (xQueueReceive(g_motion_queue, packet, pdMS_TO_TICKS(5000)) == pdTRUE) {
            packet_count++;
            printf("[Spider] Received packet #%lu\n", packet_count);
            /* TODO: When PCA9685 is added, process and output to servos */
        } else {
            /* Heartbeat every 5 seconds when idle */
            printf("[Spider] Heartbeat - waiting for commands...\n");
        }
    }
}

void app_main(void) {
    printf("[Spider] ================================\n");
    printf("[Spider] Muscle Runtime v3.1\n");
    printf("[Spider] Simple Mode (no I2C/servos)\n");
    printf("[Spider] ================================\n");
    
    /* Create motion packet queue */
    g_motion_queue = xQueueCreate(MOTION_QUEUE_LENGTH, 64);
    if (g_motion_queue == NULL) {
        printf("[Spider] ERROR: Failed to create queue!\n");
        return;
    }
    printf("[Spider] Motion queue OK\n");
    
    /* Create motion processing task */
    BaseType_t ret = xTaskCreate(
        motion_task_entry,
        "motion",
        MOTION_TASK_STACK,
        NULL,
        MOTION_TASK_PRIORITY,
        NULL
    );
    
    if (ret != pdPASS) {
        printf("[Spider] ERROR: Failed to create task!\n");
        return;
    }
    
    printf("[Spider] Motion task OK\n");
    printf("[Spider] Ready!\n");
}
