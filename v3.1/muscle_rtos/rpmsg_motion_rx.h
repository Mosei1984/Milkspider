#ifndef RPMSG_MOTION_RX_H
#define RPMSG_MOTION_RX_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>
#include "shared_motion_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize RPMsg motion receiver for Milk-V Duo.
 * 
 * Uses CVITEK's mailbox + shared memory mechanism:
 * - Shared memory ring buffer at 0x83F00000 (see shared_motion_buffer.h)
 * - Mailbox notification when new packets arrive
 * - Packets are validated and pushed to the motion queue
 * 
 * @param motion_queue FreeRTOS queue to receive validated PosePacket31
 * @return 0 on success, negative on error
 */
int rpmsg_motion_rx_init(QueueHandle_t motion_queue);

/**
 * Deinitialize RPMsg receiver.
 * Unregisters mailbox IRQ handler.
 */
void rpmsg_motion_rx_deinit(void);

/**
 * Get total number of successfully received packets.
 */
uint32_t rpmsg_motion_rx_get_count(void);

/**
 * Get last successfully received sequence number.
 */
uint32_t rpmsg_motion_rx_get_last_seq(void);

/**
 * Get number of dropped packets (CRC/validation errors).
 */
uint32_t rpmsg_motion_rx_get_drop_count(void);

/**
 * Check if Brain (Linux) side is ready.
 * @return 1 if SHARED_FLAG_BRAIN_READY is set, 0 otherwise
 */
int rpmsg_motion_rx_brain_ready(void);

#ifdef MILKV_DUO_SDK
#include "rtos_cmdqu.h"
/**
 * Process a motion command from the mailbox.
 * Called from comm_main.c when IP_SYSTEM + CMD_MOTION_PACKET is received.
 */
int rpmsg_motion_process_cmd(const cmdqu_t *cmdq);
#endif

#ifdef __cplusplus
}
#endif

#endif // RPMSG_MOTION_RX_H
