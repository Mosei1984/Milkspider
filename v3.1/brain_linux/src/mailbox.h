/**
 * Spider Robot v3.1 - Mailbox Communication
 * 
 * CVITEK mailbox interface for Milk-V Duo Linux ↔ FreeRTOS IPC
 * Uses /dev/cvi-rtos-cmdqu device driver
 */

#ifndef MAILBOX_H
#define MAILBOX_H

#include <cstdint>
#include <cstddef>

#define RTOS_CMDQU_DEV      "/dev/cvi-rtos-cmdqu"

#define RTOS_CMDQU_SEND     _IOW('r', 1, unsigned long)
#define RTOS_CMDQU_SEND_WAIT _IOW('r', 4, unsigned long)

#define CMD_MOTION_PACKET   0x20
#define CMD_MOTION_ACK      0x21
#define CMD_HEARTBEAT       0x22
#define CMD_ESTOP           0x23

union resv_t {
    struct {
        uint8_t linux_valid;
        uint8_t rtos_valid;
    } valid;
    uint16_t mstime;
};

struct cmdqu_t {
    uint8_t ip_id;
    uint8_t cmd_id : 7;
    uint8_t block  : 1;
    union resv_t resv;
    uint32_t param_ptr;
} __attribute__((packed));

class Mailbox {
public:
    Mailbox() = default;
    ~Mailbox();

    bool open();
    void close();
    bool isOpen() const { return m_fd >= 0; }

    bool sendCommand(uint8_t cmd_id, uint32_t param = 0, bool blocking = false);
    bool notifyPacketReady(uint32_t write_idx);
    bool sendEstop();
    bool sendHeartbeat();

    int getFd() const { return m_fd; }
    uint32_t getTxCount() const { return m_tx_count; }

private:
    int m_fd = -1;
    uint32_t m_tx_count = 0;
};

#endif // MAILBOX_H
