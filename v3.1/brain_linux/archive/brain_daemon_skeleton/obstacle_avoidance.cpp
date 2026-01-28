/**
 * ObstacleAvoidance Implementation
 */

#include "obstacle_avoidance.hpp"

#include <chrono>
#include <cmath>
#include <sstream>
#include <algorithm>

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

ObstacleAvoidance::ObstacleAvoidance(ScanController &scan, EyeClientUnix &eyes)
    : m_scan(scan), m_eyes(eyes) {
}

void ObstacleAvoidance::setThresholds(int warn_mm, int stop_mm, int critical_mm) {
    m_warn_threshold_mm = warn_mm;
    m_stop_threshold_mm = stop_mm;
    m_critical_threshold_mm = critical_mm;
}

void ObstacleAvoidance::tick() {
    if (!m_enabled || !m_scan.isRunning()) {
        m_reaction.obstacle_detected = false;
        return;
    }

    m_reaction = Reaction();

    const auto &data = m_scan.getScanData();
    if (data.empty()) return;

    int left_min = 9999, front_min = 9999, right_min = 9999;

    for (const auto &p : data) {
        if (p.distance_mm <= 0) continue;

        if (p.angle_deg >= -60 && p.angle_deg < -20) {
            if (p.distance_mm < left_min) left_min = p.distance_mm;
        } else if (p.angle_deg >= -20 && p.angle_deg <= 20) {
            if (p.distance_mm < front_min) front_min = p.distance_mm;
        } else if (p.angle_deg > 20 && p.angle_deg <= 60) {
            if (p.distance_mm < right_min) right_min = p.distance_mm;
        }
    }

    int closest = std::min({left_min, front_min, right_min});
    if (closest >= m_warn_threshold_mm) {
        m_reaction.obstacle_detected = false;
        return;
    }

    m_reaction.obstacle_detected = true;
    m_reaction.distance_mm = closest;

    if (closest <= m_critical_threshold_mm) {
        m_reaction.severity = 1.0f;
    } else if (closest <= m_stop_threshold_mm) {
        m_reaction.severity = 0.7f + 0.3f * (1.0f - (float)(closest - m_critical_threshold_mm) /
                                              (m_stop_threshold_mm - m_critical_threshold_mm));
    } else {
        m_reaction.severity = 0.3f + 0.4f * (1.0f - (float)(closest - m_stop_threshold_mm) /
                                              (m_warn_threshold_mm - m_stop_threshold_mm));
    }

    if (front_min == closest) {
        m_reaction.direction = "front";
        if (closest <= m_critical_threshold_mm) {
            m_reaction.action = Reaction::BACKUP;
        } else if (closest <= m_stop_threshold_mm) {
            m_reaction.action = Reaction::STOP;
        } else {
            m_reaction.action = Reaction::SLOW_DOWN;
        }
    } else if (left_min < right_min) {
        m_reaction.direction = "left";
        m_reaction.action = Reaction::TURN_RIGHT;
    } else {
        m_reaction.direction = "right";
        m_reaction.action = Reaction::TURN_LEFT;
    }

    updateEyes();
}

void ObstacleAvoidance::updateEyes() {
    uint64_t now = now_ms();
    if (now - m_last_eye_update_ms < m_event_rate_limit_ms) {
        return;
    }
    m_last_eye_update_ms = now;

    std::string mood = severityToMood(m_reaction.severity);

    float look_x = 0.0f;
    if (m_reaction.direction == "left") look_x = -0.6f;
    else if (m_reaction.direction == "right") look_x = 0.6f;

    std::ostringstream ss;
    ss << "{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{"
       << "\"cmd\":\"mood\",\"mood\":\"" << mood << "\""
       << "}}";
    m_eyes.sendEvent(ss.str());

    std::ostringstream ss2;
    ss2 << "{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{"
        << "\"cmd\":\"look\","
        << "\"L\":{\"x\":" << look_x << ",\"y\":0},"
        << "\"R\":{\"x\":" << look_x << ",\"y\":0}"
        << "}}";
    m_eyes.sendEvent(ss2.str());
}

std::string ObstacleAvoidance::severityToMood(float severity) {
    if (severity >= 0.8f) return "angry";
    if (severity >= 0.5f) return "suspicious";
    return "neutral";
}
