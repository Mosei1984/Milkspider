#ifndef BOOT_DEMO_HPP
#define BOOT_DEMO_HPP

#include <cstdint>
#include <string>
#include "motion_loader.hpp"
#include "eye_client_unix.hpp"
#include "ipc_muscle_rpmsg.hpp"

/**
 * BootDemo - Automatic startup animation sequence
 *
 * Timeline:
 * 1. Eyes: fade in, blink, look around
 * 2. Servos: smooth ramp to wakepose
 * 3. Idle: hand off to normal operation
 *
 * Aborts immediately on manual input or ESTOP.
 */
class BootDemo {
public:
    BootDemo(MotionLoader &motion_loader, EyeClientUnix &eye_client,
             IpcMuscleRpmsg &muscle_ipc);

    enum class WakePose {
        DEFAULT,
        COMBAT,
        PUSHUP,
        HELLO,
        DANCE
    };

    void start(WakePose pose = WakePose::DEFAULT);
    void abort();
    void tick();

    bool isRunning() const { return m_running; }
    bool isComplete() const { return m_complete; }

private:
    enum class Phase {
        IDLE,
        EYE_WAKE,
        EYE_BLINK,
        EYE_LOOK,
        SERVO_RAMP,
        COMPLETE
    };

    void tickEyeWake();
    void tickEyeBlink();
    void tickEyeLook();
    void tickServoRamp();
    void sendEyeCommand(const std::string &cmd);

    MotionLoader &m_motion_loader;
    EyeClientUnix &m_eye_client;
    IpcMuscleRpmsg &m_muscle_ipc;

    Phase m_phase = Phase::IDLE;
    bool m_running = false;
    bool m_complete = false;
    uint64_t m_phase_start_ms = 0;
    uint32_t m_tick_count = 0;

    WakePose m_wakepose = WakePose::DEFAULT;
    MotionSequenceIterator m_servo_iterator;
    uint64_t m_frame_start_ms = 0;
    uint32_t m_packet_seq = 0;
};

#endif // BOOT_DEMO_HPP
