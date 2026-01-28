#ifndef LIDAR_DRIVER_HPP
#define LIDAR_DRIVER_HPP

#include <cstdint>
#include <string>

/**
 * LidarDriver - I2C LiDAR sensor interface
 *
 * Supports VL53L0X (default) and similar ToF sensors.
 */
class LidarDriver {
public:
    LidarDriver();
    ~LidarDriver();

    bool init(const std::string &i2c_device = "/dev/i2c-2", uint8_t address = 0x29);
    void close();

    int readDistanceMm();

    bool isReady() const { return m_fd >= 0; }

    void setAddress(uint8_t address) { m_address = address; }

private:
    bool writeReg(uint8_t reg, uint8_t value);
    bool writeReg16(uint8_t reg, uint16_t value);
    uint8_t readReg(uint8_t reg);
    uint16_t readReg16(uint8_t reg);

    bool initVL53L0X();

    int m_fd = -1;
    uint8_t m_address = 0x29;
};

#endif // LIDAR_DRIVER_HPP
