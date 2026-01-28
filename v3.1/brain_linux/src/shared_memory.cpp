/**
 * Spider Robot v3.1 - Shared Memory Implementation
 */

#include "shared_memory.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>
#include <iostream>

extern "C" {
#include "crc16_ccitt_false.h"
}

SharedMemory::~SharedMemory() {
    unmap();
}

bool SharedMemory::map() {
    if (m_buffer != nullptr) {
        return true;
    }

    m_mem_fd = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (m_mem_fd < 0) {
        std::cerr << "[SharedMem] Failed to open /dev/mem: " << strerror(errno) << std::endl;
        return false;
    }

    void* ptr = mmap(nullptr, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, m_mem_fd, SHARED_MEM_BASE);
    
    if (ptr == MAP_FAILED) {
        std::cerr << "[SharedMem] mmap failed at 0x" << std::hex << SHARED_MEM_BASE 
                  << std::dec << ": " << strerror(errno) << std::endl;
        ::close(m_mem_fd);
        m_mem_fd = -1;
        return false;
    }

    m_buffer = static_cast<SharedMotionBuffer*>(ptr);

    m_buffer->write_idx = 0;
    m_buffer->read_idx = 0;
    m_buffer->flags = 0;
    m_buffer->reserved = 0;
    memset((void*)m_buffer->packets, 0, sizeof(m_buffer->packets));

    __sync_synchronize();

    std::cout << "[SharedMem] Mapped at 0x" << std::hex << SHARED_MEM_BASE 
              << std::dec << " (" << SHARED_MEM_SIZE << " bytes)" << std::endl;
    return true;
}

void SharedMemory::unmap() {
    if (m_buffer != nullptr) {
        munmap(m_buffer, SHARED_MEM_SIZE);
        m_buffer = nullptr;
    }
    if (m_mem_fd >= 0) {
        ::close(m_mem_fd);
        m_mem_fd = -1;
    }
}

bool SharedMemory::writePacket(const PosePacket31* pkt, uint32_t& out_write_idx) {
    if (m_buffer == nullptr || pkt == nullptr) {
        return false;
    }

    uint32_t write_idx = m_buffer->write_idx;
    uint32_t read_idx = m_buffer->read_idx;

    if ((write_idx - read_idx) >= PACKET_RING_SIZE) {
        std::cerr << "[SharedMem] Ring buffer full (w=" << write_idx 
                  << " r=" << read_idx << ")" << std::endl;
        return false;
    }

    uint32_t slot = write_idx % PACKET_RING_SIZE;
    
    memcpy((void*)m_buffer->packets[slot], pkt, sizeof(PosePacket31));

    __sync_synchronize();

    m_buffer->write_idx = write_idx + 1;

    __sync_synchronize();

    out_write_idx = write_idx + 1;
    return true;
}

bool SharedMemory::isFull() const {
    if (m_buffer == nullptr) return true;
    return (m_buffer->write_idx - m_buffer->read_idx) >= PACKET_RING_SIZE;
}

uint32_t SharedMemory::available() const {
    if (m_buffer == nullptr) return 0;
    uint32_t used = m_buffer->write_idx - m_buffer->read_idx;
    return (used < PACKET_RING_SIZE) ? (PACKET_RING_SIZE - used) : 0;
}

uint32_t SharedMemory::getWriteIdx() const {
    return m_buffer ? m_buffer->write_idx : 0;
}

uint32_t SharedMemory::getReadIdx() const {
    return m_buffer ? m_buffer->read_idx : 0;
}
