/**
 * ScanController Implementation
 */

#include "scan_controller.hpp"
#include "../../common/protocol_posepacket31.h"
#include "../../common/crc16_ccitt_false.h"
#include "../../common/limits.h"

#include <chrono>
#include <algorithm>

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

ScanController::ScanController(LidarDriver &lidar, IpcMuscleRpmsg &muscle)
    : m_lidar(lidar), m_muscle(muscle) {
}

void ScanController::start() {
    m_running = true;
    m_current_angle = m_profile.min_deg;
    m_direction = 1;
    m_scan_data.clear();
    m_last_measure_ms = now_ms();
    m_dwelling = false;

    moveToAngle(m_current_angle);
    m_dwell_start_ms = now_ms();
    m_dwelling = true;
}

void ScanController::stop() {
    m_running = false;
    moveToAngle(0);
}

void ScanController::tick() {
    if (!m_running) return;

    uint64_t now = now_ms();
    uint32_t period_ms = 1000 / m_profile.rate_hz;

    if (m_dwelling) {
        if (now - m_dwell_start_ms >= (uint64_t)m_profile.dwell_ms) {
            m_dwelling = false;
        }
        return;
    }

    if (now - m_last_measure_ms >= period_ms) {
        int distance = m_lidar.readDistanceMm();

        ScanPoint point;
        point.angle_deg = m_current_angle;
        point.distance_mm = distance;

        bool found = false;
        for (auto &p : m_scan_data) {
            if (p.angle_deg == m_current_angle) {
                p.distance_mm = distance;
                found = true;
                break;
            }
        }
        if (!found) {
            m_scan_data.push_back(point);
        }

        m_current_angle += m_direction * m_profile.step_deg;

        if (m_current_angle > m_profile.max_deg) {
            m_current_angle = m_profile.max_deg;
            m_direction = -1;
        } else if (m_current_angle < m_profile.min_deg) {
            m_current_angle = m_profile.min_deg;
            m_direction = 1;
        }

        moveToAngle(m_current_angle);
        m_dwell_start_ms = now;
        m_dwelling = true;

        m_last_measure_ms = now;
    }
}

void ScanController::moveToAngle(int deg) {
    int servo_us = angleToServoUs(deg);

    PosePacket31 pkt;
    posepacket31_init(&pkt, m_packet_seq++);

    pkt.servo_us[SERVO_CHANNEL_SCAN] = (uint16_t)servo_us;
    pkt.t_ms = 50;
    pkt.flags = FLAG_CLAMP_ENABLE | FLAG_SCAN_ENABLE | FLAG_HOLD;

    pkt.crc16 = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);

    m_muscle.send(&pkt, sizeof(pkt));
}

int ScanController::angleToServoUs(int deg) {
    int center = 1500;
    int range = 1000;

    int us = center + (deg * range / 90);

    if (us < SERVO_PWM_MIN_US) us = SERVO_PWM_MIN_US;
    if (us > SERVO_PWM_MAX_US) us = SERVO_PWM_MAX_US;

    return us;
}

int ScanController::getClosestDistance() const {
    int closest = 9999;
    for (const auto &p : m_scan_data) {
        if (p.distance_mm > 0 && p.distance_mm < closest) {
            closest = p.distance_mm;
        }
    }
    return closest;
}

int ScanController::getClosestAngle() const {
    int closest_dist = 9999;
    int closest_angle = 0;
    for (const auto &p : m_scan_data) {
        if (p.distance_mm > 0 && p.distance_mm < closest_dist) {
            closest_dist = p.distance_mm;
            closest_angle = p.angle_deg;
        }
    }
    return closest_angle;
}
