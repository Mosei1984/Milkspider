/**
 * RPMsg Motion Receiver for Milk-V Duo
 *
 * Receives PosePacket31 from Brain (Linux) via shared memory + mailbox.
 * 
 * Milk-V Duo uses CVITEK's rtos_cmdqu mechanism:
 * - Mailbox: 8-byte notification with param_ptr pointing to shared memory
 * - Shared memory: Ring buffer at 0x83F00000 (see shared_motion_buffer.h)
 * - Uses IP_SYSTEM with custom cmd_id for motion packets
 * 
 * Flow:
 * 1. Linux writes packet to shared memory ring buffer
 * 2. Linux sends mailbox notification (CMD_MOTION_PACKET)
 * 3. FreeRTOS receives IRQ, calls rpmsg_motion_process_cmd()
 * 4. Packets are validated and queued for motion task
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "rpmsg_motion_rx.h"
#include "shared_motion_buffer.h"
#include "protocol_posepacket31.h"
#include "crc16_ccitt_false.h"
#include "fault_flags.h"

#ifdef MILKV_DUO_SDK
#include "rtos_cmdqu.h"
#endif

static QueueHandle_t s_motion_queue = NULL;
static volatile SharedMotionBuffer *s_shared_buf = NULL;
static volatile uint32_t s_last_seq = 0;
static volatile uint32_t s_rx_count = 0;
static volatile uint32_t s_drop_count = 0;

static int validate_and_queue_packet(const PosePacket31 *pkt, BaseType_t from_isr) {
    if (pkt->magic != SPIDER_MAGIC) {
        fault_flags_set(FAULT_PACKET_MAGIC);
        s_drop_count++;
        return -1;
    }

    if (pkt->ver_major != SPIDER_VERSION_MAJOR || 
        pkt->ver_minor != SPIDER_VERSION_MINOR) {
        fault_flags_set(FAULT_PACKET_VERSION);
        s_drop_count++;
        return -2;
    }

    uint16_t computed_crc = crc16_ccitt_false((const uint8_t *)pkt, 
                                               sizeof(PosePacket31) - 2);
    if (computed_crc != pkt->crc16) {
        fault_flags_set(FAULT_PACKET_CRC);
        s_drop_count++;
        return -3;
    }

    if (pkt->seq <= s_last_seq && s_last_seq != 0) {
        s_drop_count++;
        return -4;
    }
    s_last_seq = pkt->seq;

    if (s_motion_queue != NULL) {
        if (from_isr) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            if (xQueueSendFromISR(s_motion_queue, pkt, &xHigherPriorityTaskWoken) == pdPASS) {
                s_rx_count++;
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                return 0;
            }
        } else {
            if (xQueueSend(s_motion_queue, pkt, 0) == pdPASS) {
                s_rx_count++;
                return 0;
            }
        }
    }
    s_drop_count++;
    return -5;
}

#ifdef MILKV_DUO_SDK

/**
 * Process motion command from mailbox IRQ.
 * Called from comm_main.c's prvCmdQuRunTask when a motion command is received.
 * 
 * Drains all available packets from the ring buffer since the mailbox
 * notification may batch multiple writes.
 * 
 * @param cmdq The command received via mailbox
 * @return Number of packets processed, or negative on error
 */
int rpmsg_motion_process_cmd(const cmdqu_t *cmdq) {
    if (cmdq->cmd_id != CMD_MOTION_PACKET) {
        return -1;
    }

    if (s_shared_buf == NULL) {
        return -2;
    }

    __asm volatile ("dmb" ::: "memory");
    
    uint32_t read_idx = s_shared_buf->read_idx;
    uint32_t write_idx = s_shared_buf->write_idx;
    int processed = 0;
    
    while (read_idx != write_idx) {
        uint32_t slot = shared_buffer_slot_index(read_idx);
        const PosePacket31 *pkt = 
            (const PosePacket31 *)s_shared_buf->packets[slot];
        
        validate_and_queue_packet(pkt, pdTRUE);
        processed++;
        
        read_idx++;
        s_shared_buf->read_idx = read_idx;
        __asm volatile ("dmb" ::: "memory");
    }

    return processed;
}

#else

static int rpmsg_motion_callback(void *payload, size_t len, uint32_t src, void *priv) {
    (void)src;
    (void)priv;

    if (len != sizeof(PosePacket31)) {
        fault_flags_set(FAULT_RPMSG_SIZE);
        return 0;
    }

    validate_and_queue_packet((const PosePacket31 *)payload, pdFALSE);
    return 0;
}

#endif

int rpmsg_motion_rx_init(QueueHandle_t motion_queue) {
    s_motion_queue = motion_queue;
    s_last_seq = 0;
    s_rx_count = 0;
    s_drop_count = 0;

#ifdef MILKV_DUO_SDK
    s_shared_buf = (volatile SharedMotionBuffer *)SHARED_MEM_BASE;
    
    s_shared_buf->read_idx = 0;
    s_shared_buf->flags |= SHARED_FLAG_MUSCLE_READY;
    __asm volatile ("dmb" ::: "memory");
    
    printf("[Motion RX] Shared memory ring buffer at 0x%08lX (%u bytes)\n", 
           (unsigned long)SHARED_MEM_BASE, (unsigned)sizeof(SharedMotionBuffer));
    printf("[Motion RX] Waiting for Brain ready flag...\n");
#else
    printf("[Motion RX] Stub mode - no SDK available\n");
#endif

    return 0;
}

void rpmsg_motion_rx_deinit(void) {
#ifdef MILKV_DUO_SDK
    if (s_shared_buf != NULL) {
        s_shared_buf->flags &= ~SHARED_FLAG_MUSCLE_READY;
        __asm volatile ("dmb" ::: "memory");
    }
#endif
    s_motion_queue = NULL;
    s_shared_buf = NULL;
}

uint32_t rpmsg_motion_rx_get_count(void) {
    return s_rx_count;
}

uint32_t rpmsg_motion_rx_get_last_seq(void) {
    return s_last_seq;
}

uint32_t rpmsg_motion_rx_get_drop_count(void) {
    return s_drop_count;
}

int rpmsg_motion_rx_brain_ready(void) {
#ifdef MILKV_DUO_SDK
    if (s_shared_buf == NULL) {
        return 0;
    }
    return (s_shared_buf->flags & SHARED_FLAG_BRAIN_READY) ? 1 : 0;
#else
    return 0;
#endif
}
