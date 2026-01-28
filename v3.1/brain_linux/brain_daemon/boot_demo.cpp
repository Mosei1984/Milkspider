/**
 * BootDemo Implementation
 */

#include "boot_demo.hpp"
#include "../../common/protocol_posepacket31.h"
#include "../../common/crc16_ccitt_false.h"
#include "../../common/limits.h"

#include <chrono>
#include <cstring>

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

BootDemo::BootDemo(MotionLoader &motion_loader, EyeClientUnix &eye_client,
                   IpcMuscleRpmsg &muscle_ipc)
    : m_motion_loader(motion_loader), m_eye_client(eye_client),
      m_muscle_ipc(muscle_ipc) {
}

void BootDemo::start(WakePose pose) {
    m_wakepose = pose;
    m_running = true;
    m_complete = false;
    m_phase = Phase::EYE_WAKE;
    m_phase_start_ms = now_ms();
    m_tick_count = 0;

    sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"backlight\",\"bl\":0}}");
}

void BootDemo::abort() {
    m_running = false;
    m_phase = Phase::IDLE;
}

void BootDemo::tick() {
    if (!m_running) return;

    m_tick_count++;

    switch (m_phase) {
        case Phase::EYE_WAKE:
            tickEyeWake();
            break;
        case Phase::EYE_BLINK:
            tickEyeBlink();
            break;
        case Phase::EYE_LOOK:
            tickEyeLook();
            break;
        case Phase::SERVO_RAMP:
            tickServoRamp();
            break;
        case Phase::COMPLETE:
            m_running = false;
            m_complete = true;
            break;
        case Phase::IDLE:
        default:
            break;
    }
}

void BootDemo::tickEyeWake() {
    uint64_t elapsed = now_ms() - m_phase_start_ms;

    if (elapsed < 500) {
        int bl = (int)(elapsed * 180 / 500);
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
            "{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"backlight\",\"bl\":%d}}", bl);
        sendEyeCommand(cmd);
    } else if (elapsed < 1000) {
        sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"backlight\",\"bl\":180}}");
    } else {
        m_phase = Phase::EYE_BLINK;
        m_phase_start_ms = now_ms();
    }
}

void BootDemo::tickEyeBlink() {
    uint64_t elapsed = now_ms() - m_phase_start_ms;

    if (elapsed < 200) {
        sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"blink\"}}");
    } else if (elapsed < 600) {
    } else {
        m_phase = Phase::EYE_LOOK;
        m_phase_start_ms = now_ms();
    }
}

void BootDemo::tickEyeLook() {
    uint64_t elapsed = now_ms() - m_phase_start_ms;

    if (elapsed < 400) {
        sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"look\",\"L\":{\"x\":-0.5,\"y\":0},\"R\":{\"x\":-0.5,\"y\":0}}}");
    } else if (elapsed < 800) {
        sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"look\",\"L\":{\"x\":0.5,\"y\":0},\"R\":{\"x\":0.5,\"y\":0}}}");
    } else if (elapsed < 1200) {
        sendEyeCommand("{\"v\":\"3.1\",\"type\":\"eyes\",\"msg\":{\"cmd\":\"look\",\"L\":{\"x\":0,\"y\":0},\"R\":{\"x\":0,\"y\":0}}}");
    } else {
        m_phase = Phase::SERVO_RAMP;
        m_phase_start_ms = now_ms();

        const char *pose_name = "standby";
        switch (m_wakepose) {
            case WakePose::COMBAT:  pose_name = "fighting"; break;
            case WakePose::PUSHUP:  pose_name = "pushup"; break;
            case WakePose::HELLO:   pose_name = "hello"; break;
            case WakePose::DANCE:   pose_name = "dance1"; break;
            default: break;
        }

        const MotionSequence *seq = m_motion_loader.getSequence(pose_name);
        if (seq) {
            m_servo_iterator = MotionSequenceIterator(seq);
        }
        m_frame_start_ms = now_ms();
    }
}

void BootDemo::tickServoRamp() {
    if (!m_servo_iterator.hasNext()) {
        m_phase = Phase::COMPLETE;
        return;
    }

    uint64_t now = now_ms();
    const MotionFrame& frame = m_servo_iterator.current();

    if (now - m_frame_start_ms >= frame.t_ms) {
        m_servo_iterator.next();
        m_frame_start_ms = now;
        if (m_servo_iterator.isComplete()) {
            m_phase = Phase::COMPLETE;
            return;
        }
    }

    const MotionFrame& current_frame = m_servo_iterator.current();
    
    PosePacket31 pkt;
    posepacket31_init(&pkt, m_packet_seq++);

    uint16_t servo_us_13[SERVO_COUNT_TOTAL];
    MotionLoader::convertLegacyToPose(current_frame, servo_us_13);

    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt.servo_us[i] = servo_us_13[i];
    }

    pkt.t_ms = current_frame.t_ms;
    pkt.flags = FLAG_CLAMP_ENABLE;
    pkt.crc16 = crc16_ccitt_false((const uint8_t *)&pkt, sizeof(pkt) - 2);

    m_muscle_ipc.send(&pkt, sizeof(pkt));
}

void BootDemo::sendEyeCommand(const std::string &cmd) {
    m_eye_client.sendEvent(cmd);
}
