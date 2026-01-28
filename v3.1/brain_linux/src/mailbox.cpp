/**
 * Spider Robot v3.1 - Mailbox Communication Implementation
 */

#include "mailbox.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cerrno>
#include <iostream>

Mailbox::~Mailbox() {
    close();
}

bool Mailbox::open() {
    if (m_fd >= 0) {
        return true;
    }

    m_fd = ::open(RTOS_CMDQU_DEV, O_RDWR);
    if (m_fd < 0) {
        std::cerr << "[Mailbox] Failed to open " << RTOS_CMDQU_DEV 
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "[Mailbox] Opened " << RTOS_CMDQU_DEV << " (fd=" << m_fd << ")" << std::endl;
    return true;
}

void Mailbox::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool Mailbox::sendCommand(uint8_t cmd_id, uint32_t param, bool blocking) {
    if (m_fd < 0) {
        return false;
    }

    cmdqu_t cmd = {};
    cmd.ip_id = 0;
    cmd.cmd_id = cmd_id;
    cmd.block = blocking ? 1 : 0;
    cmd.resv.mstime = 0;
    cmd.param_ptr = param;

    unsigned long ioctl_cmd = blocking ? RTOS_CMDQU_SEND_WAIT : RTOS_CMDQU_SEND;
    int ret = ioctl(m_fd, ioctl_cmd, &cmd);
    
    if (ret < 0) {
        std::cerr << "[Mailbox] ioctl failed (cmd_id=0x" << std::hex << (int)cmd_id 
                  << std::dec << "): " << strerror(errno) << std::endl;
        return false;
    }

    m_tx_count++;
    return true;
}

bool Mailbox::notifyPacketReady(uint32_t write_idx) {
    return sendCommand(CMD_MOTION_PACKET, write_idx, false);
}

bool Mailbox::sendEstop() {
    return sendCommand(CMD_ESTOP, 0, false);
}

bool Mailbox::sendHeartbeat() {
    return sendCommand(CMD_HEARTBEAT, 0, false);
}
