/**
 * TaskManager Implementation
 */

#include "task_manager.hpp"
#include "../../common/limits.h"
#include "../../common/protocol_posepacket31.h"
#include "../../common/crc16_ccitt_false.h"

#include <chrono>
#include <cstring>

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

TaskManager::TaskManager(IpcMuscleRpmsg &muscle, EyeClientUnix &eye,
                         ConfigStore &config, Telemetry &telemetry)
    : m_muscle(muscle), m_eye(eye), m_config(config), m_telemetry(telemetry) {
    // Initialize all servos to neutral
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        m_current_pose_us[i] = SERVO_PWM_NEUTRAL_US;
    }
    m_last_heartbeat_ms = now_ms();
}

void TaskManager::tick() {
    uint64_t now = now_ms();

    // Always send heartbeat at minimum rate (every 100 ms)
    if (now - m_last_heartbeat_ms >= 100) {
        sendHeartbeat();
        m_last_heartbeat_ms = now;
    }

    // Process command queue
    processMotionQueue();

    // Run current mode
    switch (m_mode) {
        case MotionMode::LEGACY_PRG:
            runLegacyProgram();
            break;
        case MotionMode::PHASE_ENGINE:
            // TODO: Phase-based gait
            break;
        case MotionMode::DYNAMIC_GEN:
            // TODO: Dynamic generation
            break;
        case MotionMode::IDLE:
        default:
            break;
    }
}

void TaskManager::queueMotion(const MotionCommand &cmd) {
    if (m_estop_active) {
        return;  // Ignore commands during ESTOP
    }
    m_command_queue.push(cmd);
}

void TaskManager::stop() {
    // Clear queue and go to idle
    while (!m_command_queue.empty()) {
        m_command_queue.pop();
    }
    m_mode = MotionMode::IDLE;
}

void TaskManager::estop() {
    m_estop_active = true;
    stop();

    // Send ESTOP packet to Muscle
    PosePacket31 pkt;
    posepacket31_init(&pkt, m_packet_seq++);
    pkt.flags |= FLAG_ESTOP;
    pkt.crc16 = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);
    m_muscle.send(&pkt, sizeof(pkt));

    // Notify eyes
    m_eye.sendEvent("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"mood\",\"mood\":\"angry\"}}");
}

void TaskManager::clearEstop() {
    m_estop_active = false;
    m_eye.sendEvent("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"mood\",\"mood\":\"neutral\"}}");
}

bool TaskManager::loadMotionSequences(const std::string& path) {
    return m_motion_loader.loadFromFile(path);
}

void TaskManager::processMotionQueue() {
    if (m_command_queue.empty()) {
        return;
    }

    MotionCommand cmd = m_command_queue.front();
    m_command_queue.pop();

    m_sequence_continuous = cmd.continuous;

    if (m_motion_loader.hasSequence(cmd.name)) {
        startSequence(cmd.name);
        m_mode = MotionMode::LEGACY_PRG;
    }
}

void TaskManager::startSequence(const std::string& name) {
    m_current_iterator = m_motion_loader.createIterator(name);
    m_current_sequence_name = name;
    m_frame_start_ms = 0;
}

void TaskManager::runLegacyProgram() {
    uint64_t now = now_ms();

    if (m_current_iterator.isComplete()) {
        if (m_sequence_continuous && !m_current_sequence_name.empty()) {
            m_current_iterator.reset();
            m_frame_start_ms = 0;
        } else {
            m_mode = MotionMode::IDLE;
            return;
        }
    }

    if (!m_current_iterator.hasNext()) {
        return;
    }

    const MotionFrame& frame = m_current_iterator.current();

    if (m_frame_start_ms == 0) {
        MotionLoader::convertLegacyToPose(frame, m_current_pose_us);
        sendPosePacket();
        m_frame_start_ms = now;
        return;
    }

    uint64_t elapsed = now - m_frame_start_ms;
    if (elapsed >= frame.t_ms) {
        m_current_iterator.next();
        m_frame_start_ms = 0;
    }
}

void TaskManager::sendPosePacket() {
    PosePacket31 pkt;
    posepacket31_init(&pkt, m_packet_seq++);

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt.servo_us[i] = m_current_pose_us[i];
    }

    if (m_current_iterator.hasNext()) {
        pkt.t_ms = m_current_iterator.current().t_ms;
    } else {
        pkt.t_ms = 20;
    }
    pkt.flags = FLAG_CLAMP_ENABLE;

    // Calculate CRC
    pkt.crc16 = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);

    m_muscle.send(&pkt, sizeof(pkt));
}

void TaskManager::sendHeartbeat() {
    PosePacket31 pkt;
    posepacket31_init(&pkt, m_packet_seq++);

    // Heartbeat uses HOLD flag with current pose
    pkt.flags = FLAG_CLAMP_ENABLE | FLAG_HOLD;

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt.servo_us[i] = m_current_pose_us[i];
    }

    pkt.crc16 = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);

    m_muscle.send(&pkt, sizeof(pkt));
}
