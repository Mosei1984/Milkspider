#ifndef IPC_MUSCLE_RPMSG_HPP
#define IPC_MUSCLE_RPMSG_HPP

#include <cstdint>
#include <cstddef>
#include <string>

#include "shared_motion_buffer.h"

/**
 * IpcMuscleRpmsg - Brain → Muscle communication for Milk-V Duo
 *
 * Supports multiple IPC mechanisms with automatic detection:
 * 
 * 1. NATIVE_CMDQU: CVITEK's /dev/cvi-rtos-cmdqu + shared memory
 *    - Best performance, native to Milk-V Duo SDK
 *    - Uses 8-byte mailbox for notification
 *    - Shared memory ring buffer for 42-byte PosePacket31
 *    - Ring buffer at 0x83F00000 (see shared_motion_buffer.h)
 * 
 * 2. RPMSG_CTRL: Standard Linux rpmsg_ctrl driver
 *    - Creates endpoint via ioctl on /dev/rpmsg_ctrl*
 *    - Communicates via /dev/rpmsg* character device
 * 
 * 3. RPMSG_CHAR: Pre-existing rpmsg character device
 *    - Direct access to /dev/rpmsg* if already created
 * 
 * 4. LEGACY: Fallback to any rpmsg-like device in /dev/
 */
class IpcMuscleRpmsg {
public:
    enum class IpcMode {
        NONE,
        NATIVE_CMDQU,
        RPMSG_CHAR,
        RPMSG_CTRL,
        LEGACY
    };

    IpcMuscleRpmsg() = default;
    ~IpcMuscleRpmsg();

    /**
     * Initialize IPC channel.
     * Tries mechanisms in order: native cmdqu, rpmsg_ctrl, rpmsg_char, legacy.
     * @return true on success
     */
    bool init();

    /**
     * Send PosePacket31 to Muscle core.
     * @param data Pointer to packet data
     * @param len Size in bytes (should be 42 for PosePacket31)
     * @return true on success
     */
    bool send(const void *data, size_t len);

    /**
     * Receive data from Muscle core (status/telemetry channel).
     * @param buffer Receive buffer
     * @param max_len Maximum bytes to read
     * @return Bytes received, or -1 on error
     */
    int receive(void *buffer, size_t max_len);

    /**
     * Close all IPC resources.
     */
    void close();

    bool isConnected() const { return m_fd >= 0 || m_cmdqu_fd >= 0; }
    
    IpcMode getMode() const { return m_mode; }
    std::string getModeString() const;
    
    const std::string& getDevicePath() const { return m_device_path; }
    
    uint32_t getTxCount() const { return m_tx_count; }
    uint32_t getDropCount() const { return m_drop_count; }
    
    /**
     * Get ring buffer status for diagnostics.
     * @param write_idx Output: current write index
     * @param read_idx Output: current read index
     * @return true if shared memory is available
     */
    bool getRingBufferStatus(uint32_t &write_idx, uint32_t &read_idx) const;

private:
    int m_fd = -1;
    int m_cmdqu_fd = -1;
    int m_ctrl_fd = -1;
    SharedMotionBuffer *m_shared_buf = nullptr;
    
    std::string m_device_path;
    IpcMode m_mode = IpcMode::NONE;
    uint32_t m_tx_count = 0;
    uint32_t m_drop_count = 0;

    bool tryNativeCmdqu();
    bool tryRpmsgChar();
    bool tryRpmsgCtrl();
    bool tryLegacyRpmsg();
    
    bool findRpmsgDevice();
    std::string findCreatedEndpoint();
    
    bool sendViaCmdqu(const void *data, size_t len);
};

#endif // IPC_MUSCLE_RPMSG_HPP
