/**
 * IpcMuscleRpmsg Implementation for Milk-V Duo
 * 
 * Supports multiple SDK versions and IPC mechanisms:
 * 1. Native CVITEK cmdqu: /dev/cvi-rtos-cmdqu (mailbox + shared mem)
 * 2. RPMsg char: /dev/rpmsg_ctrl* + /dev/rpmsg* (standard Linux RPMsg)
 * 3. RPMsg TTY: /dev/ttyRPMSG* (if available)
 * 
 * Shared memory ring buffer is defined in shared_motion_buffer.h
 */

#include "ipc_muscle_rpmsg.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>

#define RPMSG_ENDPOINT_NAME "rpmsg_motion"
#define RTOS_CMDQU_DEV      "/dev/cvi-rtos-cmdqu"

enum SYSTEM_CMD_TYPE {
    CMDQU_SEND = 1,
    CMDQU_REQUEST,
    CMDQU_REQUEST_FREE,
    CMDQU_SEND_WAIT,
    CMDQU_SEND_WAKEUP,
};

#define RTOS_CMDQU_SEND         _IOW('r', CMDQU_SEND, unsigned long)
#define RTOS_CMDQU_SEND_WAIT    _IOW('r', CMDQU_SEND_WAIT, unsigned long)

struct cmdqu_t {
    unsigned char ip_id;
    unsigned char cmd_id : 7;
    unsigned char block : 1;
    union {
        struct {
            unsigned char linux_valid;
            unsigned char rtos_valid;
        } valid;
        unsigned short mstime;
    } resv;
    unsigned int param_ptr;
} __attribute__((packed)) __attribute__((aligned(0x8)));

struct rpmsg_endpoint_info {
    char name[32];
    uint32_t src;
    uint32_t dst;
};

#define RPMSG_CREATE_EPT_IOCTL  _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)

IpcMuscleRpmsg::~IpcMuscleRpmsg() {
    close();
}

bool IpcMuscleRpmsg::init() {
    if (tryNativeCmdqu()) {
        std::cout << "[RPMsg] Using native CVITEK cmdqu" << std::endl;
        return true;
    }
    
    if (tryRpmsgChar()) {
        std::cout << "[RPMsg] Using rpmsg_char driver" << std::endl;
        return true;
    }
    
    if (tryRpmsgCtrl()) {
        std::cout << "[RPMsg] Using rpmsg_ctrl driver" << std::endl;
        return true;
    }
    
    if (tryLegacyRpmsg()) {
        std::cout << "[RPMsg] Using legacy rpmsg device" << std::endl;
        return true;
    }
    
    std::cerr << "[RPMsg] No suitable IPC mechanism found" << std::endl;
    return false;
}

bool IpcMuscleRpmsg::tryNativeCmdqu() {
    if (access(RTOS_CMDQU_DEV, F_OK) != 0) {
        return false;
    }
    
    m_cmdqu_fd = open(RTOS_CMDQU_DEV, O_RDWR);
    if (m_cmdqu_fd < 0) {
        std::cerr << "[RPMsg] Failed to open " << RTOS_CMDQU_DEV 
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        std::cerr << "[RPMsg] Failed to open /dev/mem: " << strerror(errno) << std::endl;
        ::close(m_cmdqu_fd);
        m_cmdqu_fd = -1;
        return false;
    }
    
    void *mapped = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, mem_fd, SHARED_MEM_BASE);
    ::close(mem_fd);
    
    if (mapped == MAP_FAILED) {
        std::cerr << "[RPMsg] Failed to mmap shared memory: " << strerror(errno) << std::endl;
        ::close(m_cmdqu_fd);
        m_cmdqu_fd = -1;
        return false;
    }
    
    m_shared_buf = static_cast<SharedMotionBuffer*>(mapped);
    
    m_shared_buf->write_idx = 0;
    m_shared_buf->read_idx = 0;
    m_shared_buf->flags = SHARED_FLAG_BRAIN_READY;
    m_shared_buf->reserved = 0;
    __sync_synchronize();
    
    m_device_path = RTOS_CMDQU_DEV;
    m_mode = IpcMode::NATIVE_CMDQU;
    m_fd = m_cmdqu_fd;
    
    std::cout << "[RPMsg] Shared memory ring buffer at 0x" << std::hex 
              << SHARED_MEM_BASE << " (" << std::dec << sizeof(SharedMotionBuffer) 
              << " bytes)" << std::endl;
    return true;
}

bool IpcMuscleRpmsg::tryRpmsgChar() {
    const char *search_paths[] = {
        "/dev/rpmsg0",
        "/dev/rpmsg1",
        "/dev/rpmsg-char-0-0",
        nullptr
    };
    
    for (int i = 0; search_paths[i] != nullptr; i++) {
        if (access(search_paths[i], F_OK) == 0) {
            m_fd = open(search_paths[i], O_RDWR);
            if (m_fd >= 0) {
                m_device_path = search_paths[i];
                m_mode = IpcMode::RPMSG_CHAR;
                return true;
            }
        }
    }
    return false;
}

bool IpcMuscleRpmsg::tryRpmsgCtrl() {
    const char *ctrl_paths[] = {
        "/dev/rpmsg_ctrl0",
        "/dev/rpmsg_ctrl1",
        nullptr
    };
    
    for (int i = 0; ctrl_paths[i] != nullptr; i++) {
        if (access(ctrl_paths[i], F_OK) != 0) {
            continue;
        }
        
        int ctrl_fd = open(ctrl_paths[i], O_RDWR);
        if (ctrl_fd < 0) {
            continue;
        }
        
        struct rpmsg_endpoint_info ept_info = {};
        strncpy(ept_info.name, RPMSG_ENDPOINT_NAME, sizeof(ept_info.name) - 1);
        ept_info.src = 0x400;
        ept_info.dst = 0xFFFFFFFF;
        
        int ret = ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &ept_info);
        if (ret < 0) {
            std::cerr << "[RPMsg] Failed to create endpoint via " << ctrl_paths[i]
                      << ": " << strerror(errno) << std::endl;
            ::close(ctrl_fd);
            continue;
        }
        
        m_ctrl_fd = ctrl_fd;
        
        std::string ept_path = findCreatedEndpoint();
        if (ept_path.empty()) {
            std::cerr << "[RPMsg] Endpoint created but device not found" << std::endl;
            ::close(ctrl_fd);
            m_ctrl_fd = -1;
            continue;
        }
        
        m_fd = open(ept_path.c_str(), O_RDWR);
        if (m_fd < 0) {
            std::cerr << "[RPMsg] Failed to open endpoint " << ept_path 
                      << ": " << strerror(errno) << std::endl;
            ioctl(ctrl_fd, RPMSG_DESTROY_EPT_IOCTL, nullptr);
            ::close(ctrl_fd);
            m_ctrl_fd = -1;
            continue;
        }
        
        m_device_path = ept_path;
        m_mode = IpcMode::RPMSG_CTRL;
        std::cout << "[RPMsg] Created endpoint: " << ept_path << std::endl;
        return true;
    }
    
    return false;
}

bool IpcMuscleRpmsg::tryLegacyRpmsg() {
    DIR *dir = opendir("/dev");
    if (!dir) {
        return false;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "rpmsg", 5) == 0 ||
            strncmp(entry->d_name, "ttyRPMSG", 8) == 0) {
            
            std::string path = std::string("/dev/") + entry->d_name;
            m_fd = open(path.c_str(), O_RDWR);
            if (m_fd >= 0) {
                m_device_path = path;
                m_mode = IpcMode::LEGACY;
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

std::string IpcMuscleRpmsg::findCreatedEndpoint() {
    usleep(100000);
    
    DIR *dir = opendir("/dev");
    if (!dir) {
        return "";
    }
    
    std::string result;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "rpmsg", 5) == 0 &&
            strstr(entry->d_name, "ctrl") == nullptr) {
            result = std::string("/dev/") + entry->d_name;
            break;
        }
    }
    closedir(dir);
    return result;
}

bool IpcMuscleRpmsg::send(const void *data, size_t len) {
    if (m_mode == IpcMode::NATIVE_CMDQU) {
        return sendViaCmdqu(data, len);
    }
    
    if (m_fd < 0) {
        return false;
    }

    ssize_t written = write(m_fd, data, len);
    if (written < 0) {
        std::cerr << "[RPMsg] Write error: " << strerror(errno) << std::endl;
        return false;
    }

    return static_cast<size_t>(written) == len;
}

bool IpcMuscleRpmsg::sendViaCmdqu(const void *data, size_t len) {
    if (m_cmdqu_fd < 0 || m_shared_buf == nullptr) {
        return false;
    }
    
    if (len > PACKET_SLOT_SIZE) {
        std::cerr << "[RPMsg] Packet too large: " << len << " > " << PACKET_SLOT_SIZE << std::endl;
        return false;
    }
    
    uint32_t write_idx = m_shared_buf->write_idx;
    uint32_t read_idx = m_shared_buf->read_idx;
    
    if (shared_buffer_is_full(m_shared_buf)) {
        m_drop_count++;
        m_shared_buf->flags |= SHARED_FLAG_OVERFLOW;
        return false;
    }
    
    uint32_t slot = shared_buffer_slot_index(write_idx);
    memcpy((void*)m_shared_buf->packets[slot], data, len);
    if (len < PACKET_SLOT_SIZE) {
        memset((void*)(m_shared_buf->packets[slot] + len), 0, PACKET_SLOT_SIZE - len);
    }
    
    __sync_synchronize();
    m_shared_buf->write_idx = write_idx + 1;
    __sync_synchronize();
    
    struct cmdqu_t cmd = {};
    cmd.ip_id = 0;
    cmd.cmd_id = CMD_MOTION_PACKET;
    cmd.resv.mstime = 0;
    cmd.param_ptr = write_idx + 1;
    
    int ret = ioctl(m_cmdqu_fd, RTOS_CMDQU_SEND, &cmd);
    if (ret < 0) {
        std::cerr << "[RPMsg] ioctl CMDQU_SEND failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    m_tx_count++;
    return true;
}

bool IpcMuscleRpmsg::getRingBufferStatus(uint32_t &write_idx, uint32_t &read_idx) const {
    if (m_shared_buf == nullptr) {
        return false;
    }
    write_idx = m_shared_buf->write_idx;
    read_idx = m_shared_buf->read_idx;
    return true;
}

int IpcMuscleRpmsg::receive(void *buffer, size_t max_len) {
    if (m_fd < 0) {
        return -1;
    }

    ssize_t bytes = read(m_fd, buffer, max_len);
    return static_cast<int>(bytes);
}

void IpcMuscleRpmsg::close() {
    if (m_mode == IpcMode::RPMSG_CTRL && m_ctrl_fd >= 0) {
        ioctl(m_ctrl_fd, RPMSG_DESTROY_EPT_IOCTL, nullptr);
        ::close(m_ctrl_fd);
        m_ctrl_fd = -1;
    }
    
    if (m_fd >= 0 && m_fd != m_cmdqu_fd) {
        ::close(m_fd);
    }
    m_fd = -1;
    
    if (m_cmdqu_fd >= 0) {
        ::close(m_cmdqu_fd);
        m_cmdqu_fd = -1;
    }
    
    if (m_shared_buf != nullptr) {
        m_shared_buf->flags &= ~SHARED_FLAG_BRAIN_READY;
        __sync_synchronize();
        munmap(m_shared_buf, SHARED_MEM_SIZE);
        m_shared_buf = nullptr;
    }
    
    m_mode = IpcMode::NONE;
}

bool IpcMuscleRpmsg::findRpmsgDevice() {
    return tryRpmsgChar() || tryRpmsgCtrl() || tryLegacyRpmsg();
}

std::string IpcMuscleRpmsg::getModeString() const {
    switch (m_mode) {
        case IpcMode::NATIVE_CMDQU: return "native_cmdqu";
        case IpcMode::RPMSG_CHAR:   return "rpmsg_char";
        case IpcMode::RPMSG_CTRL:   return "rpmsg_ctrl";
        case IpcMode::LEGACY:       return "legacy";
        default:                    return "none";
    }
}
