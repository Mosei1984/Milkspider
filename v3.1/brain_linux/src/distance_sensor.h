/**
 * Spider Robot v3.1 - VL53L0X Distance Sensor
 * 
 * Time-of-Flight laser ranging sensor on I2C2.
 * Provides obstacle detection for autonomous navigation.
 */

#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include <cstdint>
#include <atomic>

#define VL53L0X_I2C_BUS     "/dev/i2c-2"
#define VL53L0X_ADDR        0x29

// Range limits
#define VL53L0X_MIN_MM      30
#define VL53L0X_MAX_MM      2000
#define VL53L0X_TIMEOUT_MS  100

// Obstacle thresholds
#define OBSTACLE_CLOSE_MM   150
#define OBSTACLE_MEDIUM_MM  400
#define OBSTACLE_FAR_MM     800

class DistanceSensor {
public:
    enum class Status {
        OK,
        TIMEOUT,
        OUT_OF_RANGE,
        ERROR,
        NOT_INITIALIZED
    };

    DistanceSensor();
    ~DistanceSensor();

    /**
     * Initialize the VL53L0X sensor.
     */
    bool init();

    /**
     * Perform a single-shot range measurement.
     * @param distance_mm Output distance in millimeters
     * @return Status code
     */
    Status readRange(uint16_t& distance_mm);

    /**
     * Get last valid distance reading.
     */
    uint16_t getLastDistance() const { return m_last_distance; }

    /**
     * Check if obstacle is within threshold.
     */
    bool isObstacleClose() const { return m_last_distance < OBSTACLE_CLOSE_MM; }
    bool isObstacleMedium() const { return m_last_distance < OBSTACLE_MEDIUM_MM; }
    bool isObstacleFar() const { return m_last_distance < OBSTACLE_FAR_MM; }

    /**
     * Check if sensor is initialized.
     */
    bool isInitialized() const { return m_initialized; }

private:
    bool writeReg8(uint8_t reg, uint8_t value);
    bool readReg8(uint8_t reg, uint8_t& value);
    bool readReg16(uint8_t reg, uint16_t& value);

    int m_fd;
    bool m_initialized;
    uint16_t m_last_distance;
};

#endif // DISTANCE_SENSOR_H
