/**
 * Spider Robot v3.1 - Shared Memory Ring Buffer
 * 
 * Physical memory at 0x83F00000 shared between Linux and FreeRTOS
 * Contains PosePacket31 ring buffer for motion commands
 */

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <cstdint>
#include <cstddef>

extern "C" {
#include "protocol_posepacket31.h"
}

#define SHARED_MEM_BASE     0x83F00000
#define SHARED_MEM_SIZE     0x1000        // 4KB
#define PACKET_RING_SIZE    8             // 8 slots in ring buffer
#define PACKET_SLOT_SIZE    64            // Each slot 64 bytes (padding for alignment)

struct SharedMotionBuffer {
    volatile uint32_t write_idx;          // Linux writes, RTOS reads
    volatile uint32_t read_idx;           // RTOS writes, Linux reads
    volatile uint32_t flags;              // Status flags
    volatile uint32_t reserved;           // Alignment padding
    volatile uint8_t packets[PACKET_RING_SIZE][PACKET_SLOT_SIZE];
};

static_assert(sizeof(SharedMotionBuffer) <= SHARED_MEM_SIZE, "Buffer exceeds shared memory");

class SharedMemory {
public:
    SharedMemory() = default;
    ~SharedMemory();

    bool map();
    void unmap();
    bool isMapped() const { return m_buffer != nullptr; }

    bool writePacket(const PosePacket31* pkt, uint32_t& out_write_idx);
    
    bool isFull() const;
    uint32_t available() const;
    
    uint32_t getWriteIdx() const;
    uint32_t getReadIdx() const;

    SharedMotionBuffer* getBuffer() { return m_buffer; }

private:
    SharedMotionBuffer* m_buffer = nullptr;
    int m_mem_fd = -1;
};

#endif // SHARED_MEMORY_H
