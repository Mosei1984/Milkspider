#ifndef OBSTACLE_AVOIDANCE_HPP
#define OBSTACLE_AVOIDANCE_HPP

#include <cstdint>
#include <string>
#include "scan_controller.hpp"
#include "eye_client_unix.hpp"

/**
 * ObstacleAvoidance - Policy engine for obstacle detection and reaction
 */
class ObstacleAvoidance {
public:
    struct Reaction {
        bool obstacle_detected = false;
        std::string direction;    // "left", "front", "right"
        int distance_mm = 0;
        float severity = 0.0f;    // 0.0 = far, 1.0 = very close

        enum Action {
            NONE,
            SLOW_DOWN,
            TURN_LEFT,
            TURN_RIGHT,
            STOP,
            BACKUP
        } action = NONE;
    };

    ObstacleAvoidance(ScanController &scan, EyeClientUnix &eyes);

    void setThresholds(int warn_mm, int stop_mm, int critical_mm);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void tick();

    const Reaction &getReaction() const { return m_reaction; }
    bool hasObstacle() const { return m_reaction.obstacle_detected; }

private:
    void analyzeZone(int angle_min, int angle_max, const std::string &zone_name);
    void updateEyes();
    std::string severityToMood(float severity);

    ScanController &m_scan;
    EyeClientUnix &m_eyes;

    bool m_enabled = true;
    Reaction m_reaction;

    int m_warn_threshold_mm = 400;
    int m_stop_threshold_mm = 200;
    int m_critical_threshold_mm = 100;

    uint64_t m_last_eye_update_ms = 0;
    uint32_t m_event_rate_limit_ms = 200;
};

#endif // OBSTACLE_AVOIDANCE_HPP
