#ifndef SCAN_CONTROLLER_H
#define SCAN_CONTROLLER_H

#include <cstdint>
#include <vector>
#include <functional>

/**
 * ScanController - Autonomous sweep of scan servo with distance readings
 *
 * Sweeps the scan servo (CH12) back and forth while capturing distance
 * readings at each position. Runs as a background task within the Brain daemon.
 *
 * Architecture note: This runs on Brain (Linux) and sends servo commands
 * to Muscle (FreeRTOS) via the existing IPC mechanism.
 */
class ScanController {
public:
    struct ScanProfile {
        int min_deg = 20;       // Minimum angle (degrees from 0)
        int max_deg = 160;      // Maximum angle (degrees from 0)
        int step_deg = 10;      // Step size in degrees
        int rate_hz = 5;        // Measurements per second
        int dwell_ms = 80;      // Settle time at each angle before reading
    };

    struct ScanPoint {
        int angle_deg;
        int distance_mm;        // -1 = error/timeout
        uint64_t timestamp_ms;
    };

    // Callback types
    using ServoCallback = std::function<void(int angle_deg)>;
    using DistanceCallback = std::function<int()>;  // Returns distance in mm, or -1 on error
    using DataCallback = std::function<void(const ScanPoint& point)>;

    ScanController();

    // Configuration
    void setProfile(const ScanProfile& profile) { m_profile = profile; }
    const ScanProfile& getProfile() const { return m_profile; }

    // Set callbacks for servo control and distance reading
    void setServoCallback(ServoCallback cb) { m_servo_cb = cb; }
    void setDistanceCallback(DistanceCallback cb) { m_distance_cb = cb; }
    void setDataCallback(DataCallback cb) { m_data_cb = cb; }

    // Control
    void start();
    void stop();
    void tick();  // Call from main loop

    // State
    bool isRunning() const { return m_running; }
    int getCurrentAngle() const { return m_current_angle; }

    // Data access
    const std::vector<ScanPoint>& getScanData() const { return m_scan_data; }
    void clearScanData() { m_scan_data.clear(); }

    // Analysis helpers
    int getClosestDistance() const;
    int getClosestAngle() const;
    int getDistanceAtAngle(int angle_deg, int tolerance_deg = 5) const;
    
    // Get average distance in a cone (for obstacle detection)
    int getAverageDistanceInCone(int center_deg, int cone_width_deg) const;

private:
    static int angleToServoUs(int angle_deg);
    static uint64_t now_ms();

    ScanProfile m_profile;
    bool m_running = false;

    int m_current_angle = 90;
    int m_direction = 1;  // 1 = increasing, -1 = decreasing

    uint64_t m_last_step_ms = 0;
    uint64_t m_dwell_start_ms = 0;
    bool m_dwelling = false;

    std::vector<ScanPoint> m_scan_data;

    ServoCallback m_servo_cb;
    DistanceCallback m_distance_cb;
    DataCallback m_data_cb;
};

#endif // SCAN_CONTROLLER_H
