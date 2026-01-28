/**
 * LidarDriver Implementation
 *
 * VL53L0X Time-of-Flight sensor driver.
 */

#include "lidar_driver.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <iostream>

#define VL53L0X_REG_IDENTIFICATION_MODEL_ID         0xC0
#define VL53L0X_REG_SYSRANGE_START                  0x00
#define VL53L0X_REG_RESULT_RANGE_STATUS             0x14
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS         0x13

#define VL53L0X_MODEL_ID                            0xEE

LidarDriver::LidarDriver() {
}

LidarDriver::~LidarDriver() {
    close();
}

bool LidarDriver::init(const std::string &i2c_device, uint8_t address) {
    m_address = address;

    m_fd = open(i2c_device.c_str(), O_RDWR);
    if (m_fd < 0) {
        std::cerr << "[LiDAR] Failed to open " << i2c_device << std::endl;
        return false;
    }

    if (ioctl(m_fd, I2C_SLAVE, m_address) < 0) {
        std::cerr << "[LiDAR] Failed to set I2C address 0x"
                  << std::hex << (int)m_address << std::endl;
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    uint8_t model_id = readReg(VL53L0X_REG_IDENTIFICATION_MODEL_ID);
    if (model_id != VL53L0X_MODEL_ID) {
        std::cerr << "[LiDAR] Unexpected model ID: 0x" << std::hex << (int)model_id << std::endl;
    }

    if (!initVL53L0X()) {
        std::cerr << "[LiDAR] Init sequence failed" << std::endl;
        return false;
    }

    std::cout << "[LiDAR] VL53L0X initialized on " << i2c_device << std::endl;
    return true;
}

void LidarDriver::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LidarDriver::initVL53L0X() {
    writeReg(0x88, 0x00);
    writeReg(0x80, 0x01);
    writeReg(0xFF, 0x01);
    writeReg(0x00, 0x00);

    writeReg(0x00, 0x01);
    writeReg(0xFF, 0x00);
    writeReg(0x80, 0x00);

    writeReg(VL53L0X_REG_SYSRANGE_START, 0x02);

    return true;
}

int LidarDriver::readDistanceMm() {
    if (m_fd < 0) return -1;

    writeReg(VL53L0X_REG_SYSRANGE_START, 0x01);

    for (int i = 0; i < 100; i++) {
        usleep(5000);
        uint8_t status = readReg(VL53L0X_REG_RESULT_RANGE_STATUS);
        if (status & 0x01) break;
    }

    uint16_t range = readReg16(VL53L0X_REG_RESULT_RANGE_STATUS + 10);

    if (range == 0 || range > 8190) {
        return -1;
    }

    return (int)range;
}

bool LidarDriver::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return write(m_fd, buf, 2) == 2;
}

bool LidarDriver::writeReg16(uint8_t reg, uint16_t value) {
    uint8_t buf[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return write(m_fd, buf, 3) == 3;
}

uint8_t LidarDriver::readReg(uint8_t reg) {
    if (write(m_fd, &reg, 1) != 1) return 0;
    uint8_t value = 0;
    read(m_fd, &value, 1);
    return value;
}

uint16_t LidarDriver::readReg16(uint8_t reg) {
    if (write(m_fd, &reg, 1) != 1) return 0;
    uint8_t buf[2] = {0, 0};
    read(m_fd, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}
