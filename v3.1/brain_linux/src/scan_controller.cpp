/**
 * ScanController Implementation
 *
 * Autonomous scan servo sweep with distance readings.
 * Ported from brain_daemon/ skeleton and adapted for src/ Brain daemon.
 */

#include "scan_controller.h"
#include "logger.h"

#include <ctime>
#include <algorithm>
#include <cmath>

extern "C" {
#include "limits.h"
}

static const char* TAG = "Scan";

uint64_t ScanController::now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

ScanController::ScanController() {
    // Default profile: 20° to 160° in 10° steps at 5Hz
}

void ScanController::start() {
    if (m_running) return;

    m_running = true;
    m_current_angle = m_profile.min_deg;
    m_direction = 1;
    m_scan_data.clear();
    m_last_step_ms = now_ms();
    m_dwelling = false;

    LOG_INFO(TAG, "Started: %d° to %d°, step=%d°, rate=%dHz",
             m_profile.min_deg, m_profile.max_deg,
             m_profile.step_deg, m_profile.rate_hz);

    // Move to start position
    if (m_servo_cb) {
        m_servo_cb(m_current_angle);
    }
    m_dwell_start_ms = now_ms();
    m_dwelling = true;
}

void ScanController::stop() {
    if (!m_running) return;

    m_running = false;

    // Return to center
    if (m_servo_cb) {
        m_servo_cb(90);
    }
    m_current_angle = 90;

    LOG_INFO(TAG, "Stopped, %zu points collected", m_scan_data.size());
}

void ScanController::tick() {
    if (!m_running) return;

    uint64_t now = now_ms();
    uint32_t period_ms = 1000 / m_profile.rate_hz;

    // Wait for dwell time before reading
    if (m_dwelling) {
        if (now - m_dwell_start_ms >= (uint64_t)m_profile.dwell_ms) {
            m_dwelling = false;

            // Take distance reading after servo has settled
            int distance = -1;
            if (m_distance_cb) {
                distance = m_distance_cb();
            }

            // Store the reading
            ScanPoint point;
            point.angle_deg = m_current_angle;
            point.distance_mm = distance;
            point.timestamp_ms = now;

            // Update or add to scan data
            bool found = false;
            for (auto& p : m_scan_data) {
                if (p.angle_deg == m_current_angle) {
                    p.distance_mm = distance;
                    p.timestamp_ms = now;
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_scan_data.push_back(point);
            }

            // Notify callback
            if (m_data_cb) {
                m_data_cb(point);
            }

            LOG_DEBUG(TAG, "Angle %d°: %dmm", m_current_angle, distance);
        }
        return;
    }

    // Check if it's time for next step
    if (now - m_last_step_ms >= period_ms) {
        // Move to next angle
        m_current_angle += m_direction * m_profile.step_deg;

        // Bounce at limits
        if (m_current_angle > m_profile.max_deg) {
            m_current_angle = m_profile.max_deg;
            m_direction = -1;
        } else if (m_current_angle < m_profile.min_deg) {
            m_current_angle = m_profile.min_deg;
            m_direction = 1;
        }

        // Move servo
        if (m_servo_cb) {
            m_servo_cb(m_current_angle);
        }

        // Start dwell period
        m_dwell_start_ms = now;
        m_dwelling = true;
        m_last_step_ms = now;
    }
}

int ScanController::angleToServoUs(int angle_deg) {
    // 0° = 500us, 90° = 1500us, 180° = 2500us
    // Linear mapping: us = 500 + (angle * 2000 / 180)
    int us = 500 + (angle_deg * 2000 / 180);

    if (us < SERVO_PWM_MIN_US) us = SERVO_PWM_MIN_US;
    if (us > SERVO_PWM_MAX_US) us = SERVO_PWM_MAX_US;

    return us;
}

int ScanController::getClosestDistance() const {
    int closest = 9999;
    for (const auto& p : m_scan_data) {
        if (p.distance_mm > 0 && p.distance_mm < closest) {
            closest = p.distance_mm;
        }
    }
    return closest == 9999 ? -1 : closest;
}

int ScanController::getClosestAngle() const {
    int closest_dist = 9999;
    int closest_angle = 90;
    for (const auto& p : m_scan_data) {
        if (p.distance_mm > 0 && p.distance_mm < closest_dist) {
            closest_dist = p.distance_mm;
            closest_angle = p.angle_deg;
        }
    }
    return closest_angle;
}

int ScanController::getDistanceAtAngle(int angle_deg, int tolerance_deg) const {
    for (const auto& p : m_scan_data) {
        if (std::abs(p.angle_deg - angle_deg) <= tolerance_deg) {
            return p.distance_mm;
        }
    }
    return -1;
}

int ScanController::getAverageDistanceInCone(int center_deg, int cone_width_deg) const {
    int sum = 0;
    int count = 0;
    int half_width = cone_width_deg / 2;

    for (const auto& p : m_scan_data) {
        if (p.distance_mm > 0 &&
            p.angle_deg >= center_deg - half_width &&
            p.angle_deg <= center_deg + half_width) {
            sum += p.distance_mm;
            count++;
        }
    }

    return count > 0 ? sum / count : -1;
}
