#ifndef TASK_MANAGER_HPP
#define TASK_MANAGER_HPP

#include <cstdint>
#include <string>
#include <queue>
#include <memory>

#include "ipc_muscle_rpmsg.hpp"
#include "eye_client_unix.hpp"
#include "config_store.hpp"
#include "telemetry.hpp"
#include "motion_loader.hpp"

/**
 * Motion mode enumeration
 */
enum class MotionMode {
    IDLE,
    LEGACY_PRG,     // Run legacy program sequence
    PHASE_ENGINE,   // Phase-based gait generation
    DYNAMIC_GEN     // Dynamic motion generation
};

/**
 * Motion command from UI
 */
struct MotionCommand {
    std::string name;       // e.g., "forward", "backward", "standby"
    float vec_fwd = 0.0f;
    float vec_strafe = 0.0f;
    float vec_turn = 0.0f;
    float stride = 1.0f;
    float speed = 0.8f;
    float lift = 0.6f;
    bool continuous = false;
};

/**
 * TaskManager - Cooperative task scheduler
 *
 * Non-blocking! All operations must complete quickly.
 * Never block the main loop.
 */
class TaskManager {
public:
    TaskManager(IpcMuscleRpmsg &muscle, EyeClientUnix &eye,
                ConfigStore &config, Telemetry &telemetry);

    // Called from main loop at high frequency
    void tick();

    // Queue a motion command
    void queueMotion(const MotionCommand &cmd);

    // Immediate stop
    void stop();

    // Emergency stop
    void estop();

    // Clear ESTOP
    void clearEstop();

    // Get current mode
    MotionMode getMode() const { return m_mode; }

    // Load motion sequences from file
    bool loadMotionSequences(const std::string& path);

    // Get motion loader for queries
    const MotionLoader& getMotionLoader() const { return m_motion_loader; }

private:
    void processMotionQueue();
    void runLegacyProgram();
    void sendPosePacket();
    void sendHeartbeat();
    void startSequence(const std::string& name);

    IpcMuscleRpmsg &m_muscle;
    EyeClientUnix &m_eye;
    ConfigStore &m_config;
    Telemetry &m_telemetry;

    MotionMode m_mode = MotionMode::IDLE;
    std::queue<MotionCommand> m_command_queue;

    uint32_t m_packet_seq = 0;
    uint64_t m_last_heartbeat_ms = 0;
    uint64_t m_frame_start_ms = 0;

    // Current pose state
    uint16_t m_current_pose_us[13];
    bool m_estop_active = false;

    // Motion sequence playback
    MotionLoader m_motion_loader;
    MotionSequenceIterator m_current_iterator;
    std::string m_current_sequence_name;
    bool m_sequence_continuous = false;
};

#endif // TASK_MANAGER_HPP
