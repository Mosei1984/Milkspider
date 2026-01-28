/**
 * Spider Robot v3.1 - VL53L0X Distance Sensor Implementation
 */

#include "distance_sensor.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <cerrno>

// VL53L0X registers
#define REG_IDENTIFICATION_MODEL_ID  0xC0
#define REG_SYSRANGE_START          0x00
#define REG_RESULT_RANGE_STATUS     0x14
#define REG_RESULT_INTERRUPT_STATUS 0x13
#define REG_SYSTEM_INTERRUPT_CLEAR  0x0B

DistanceSensor::DistanceSensor()
    : m_fd(-1)
    , m_initialized(false)
    , m_last_distance(VL53L0X_MAX_MM)
{
}

DistanceSensor::~DistanceSensor() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

bool DistanceSensor::writeReg8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return write(m_fd, buf, 2) == 2;
}

bool DistanceSensor::readReg8(uint8_t reg, uint8_t& value) {
    if (write(m_fd, &reg, 1) != 1) return false;
    return read(m_fd, &value, 1) == 1;
}

bool DistanceSensor::readReg16(uint8_t reg, uint16_t& value) {
    if (write(m_fd, &reg, 1) != 1) return false;
    uint8_t buf[2];
    if (read(m_fd, buf, 2) != 2) return false;
    value = (buf[0] << 8) | buf[1];
    return true;
}

bool DistanceSensor::init() {
    m_fd = open(VL53L0X_I2C_BUS, O_RDWR);
    if (m_fd < 0) {
        std::cerr << "[VL53L0X] Failed to open " << VL53L0X_I2C_BUS 
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    if (ioctl(m_fd, I2C_SLAVE, VL53L0X_ADDR) < 0) {
        std::cerr << "[VL53L0X] Failed to set I2C address: " << strerror(errno) << std::endl;
        close(m_fd);
        m_fd = -1;
        return false;
    }

    // Verify device ID
    uint8_t model_id = 0;
    if (!readReg8(REG_IDENTIFICATION_MODEL_ID, model_id)) {
        std::cerr << "[VL53L0X] Failed to read model ID" << std::endl;
        close(m_fd);
        m_fd = -1;
        return false;
    }

    if (model_id != 0xEE) {
        std::cerr << "[VL53L0X] Invalid model ID: 0x" << std::hex << (int)model_id 
                  << " (expected 0xEE)" << std::dec << std::endl;
        close(m_fd);
        m_fd = -1;
        return false;
    }

    // Minimal initialization sequence
    writeReg8(0x88, 0x00);
    writeReg8(0x80, 0x01);
    writeReg8(0xFF, 0x01);
    writeReg8(0x00, 0x00);
    usleep(10000);
    writeReg8(0x00, 0x01);
    writeReg8(0xFF, 0x00);
    writeReg8(0x80, 0x00);

    m_initialized = true;
    std::cout << "[VL53L0X] Initialized (model_id=0x" << std::hex << (int)model_id 
              << std::dec << ")" << std::endl;
    return true;
}

DistanceSensor::Status DistanceSensor::readRange(uint16_t& distance_mm) {
    if (!m_initialized || m_fd < 0) {
        return Status::NOT_INITIALIZED;
    }

    // Start single-shot measurement
    if (!writeReg8(REG_SYSRANGE_START, 0x01)) {
        return Status::ERROR;
    }

    // Wait for measurement to complete
    uint8_t status;
    int timeout = VL53L0X_TIMEOUT_MS;
    while (timeout > 0) {
        if (!readReg8(REG_SYSRANGE_START, status)) {
            return Status::ERROR;
        }
        if ((status & 0x01) == 0) break;
        usleep(1000);
        timeout--;
    }

    if (timeout == 0) {
        return Status::TIMEOUT;
    }

    // Wait for result ready
    timeout = VL53L0X_TIMEOUT_MS;
    while (timeout > 0) {
        if (!readReg8(REG_RESULT_RANGE_STATUS, status)) {
            return Status::ERROR;
        }
        if (status & 0x01) break;
        usleep(1000);
        timeout--;
    }

    if (timeout == 0) {
        return Status::TIMEOUT;
    }

    // Read range value (offset +10 from status register)
    if (!readReg16(REG_RESULT_RANGE_STATUS + 10, distance_mm)) {
        return Status::ERROR;
    }

    // Clear interrupt
    writeReg8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Check range validity
    if (distance_mm < VL53L0X_MIN_MM || distance_mm > VL53L0X_MAX_MM) {
        return Status::OUT_OF_RANGE;
    }

    m_last_distance = distance_mm;
    return Status::OK;
}
