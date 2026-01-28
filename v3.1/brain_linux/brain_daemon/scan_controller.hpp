#ifndef SCAN_CONTROLLER_HPP
#define SCAN_CONTROLLER_HPP

#include <cstdint>
#include <vector>
#include "lidar_driver.hpp"
#include "ipc_muscle_rpmsg.hpp"

/**
 * ScanController - Sweep LiDAR across scan servo
 *
 * Manages scan servo position and captures distance at each angle.
 */
class ScanController {
public:
    struct ScanProfile {
        int min_deg = -60;
        int max_deg = 60;
        int step_deg = 3;
        int rate_hz = 8;      // Measurements per second
        int dwell_ms = 50;    // Settle time at each angle
    };

    struct ScanPoint {
        int angle_deg;
        int distance_mm;
    };

    ScanController(LidarDriver &lidar, IpcMuscleRpmsg &muscle);

    void setProfile(const ScanProfile &profile) { m_profile = profile; }
    const ScanProfile &getProfile() const { return m_profile; }

    void start();
    void stop();
    void tick();

    bool isRunning() const { return m_running; }

    const std::vector<ScanPoint> &getScanData() const { return m_scan_data; }
    int getClosestDistance() const;
    int getClosestAngle() const;

private:
    void moveToAngle(int deg);
    int angleToServoUs(int deg);

    LidarDriver &m_lidar;
    IpcMuscleRpmsg &m_muscle;

    ScanProfile m_profile;
    bool m_running = false;

    int m_current_angle = 0;
    int m_direction = 1;  // 1 = increasing, -1 = decreasing
    uint64_t m_last_measure_ms = 0;
    uint64_t m_dwell_start_ms = 0;
    bool m_dwelling = false;

    std::vector<ScanPoint> m_scan_data;
    uint32_t m_packet_seq = 0;
};

#endif // SCAN_CONTROLLER_HPP
