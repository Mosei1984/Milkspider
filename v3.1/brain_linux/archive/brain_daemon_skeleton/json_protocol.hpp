#ifndef JSON_PROTOCOL_HPP
#define JSON_PROTOCOL_HPP

#include <string>
#include <cstdint>
#include <optional>
#include "task_manager.hpp"

/**
 * JsonProtocol - v3.1 JSON message parser and generator
 */
class JsonProtocol {
public:
    struct ParseResult {
        bool valid = false;
        std::string error;
        std::string type;
    };

    struct MotionMsg {
        std::string mode;
        std::string cmd;
        float vec_fwd = 0.0f;
        float vec_strafe = 0.0f;
        float vec_turn = 0.0f;
        float stride = 1.0f;
        float speed = 0.8f;
        float lift = 0.6f;
        std::string interp = "float";
    };

    struct EyeMsg {
        std::string cmd;
        std::string mode;
        std::string mood;
        float lx = 0.0f, ly = 0.0f;
        float rx = 0.0f, ry = 0.0f;
        std::string eye = "both";
        int backlight = 180;
    };

    struct SystemMsg {
        std::string cmd;
        std::string wakepose;
    };

    ParseResult parseMessage(const std::string &json);

    std::optional<MotionMsg> getMotionMsg() const { return m_motion_msg; }
    std::optional<EyeMsg> getEyeMsg() const { return m_eye_msg; }
    std::optional<SystemMsg> getSystemMsg() const { return m_system_msg; }

    static std::string createTelemetryResponse(uint32_t uptime_s, float loop_hz,
                                               uint32_t packets_sent, const std::string &state);
    static std::string createErrorResponse(int code, const std::string &message);
    static std::string createAckResponse(const std::string &cmd);

    MotionCommand toMotionCommand() const;

private:
    bool validateEnvelope(const std::string &json);
    bool parseMotion(const std::string &msg_json);
    bool parseEyes(const std::string &msg_json);
    bool parseSystem(const std::string &msg_json);

    float clampFloat(float val, float min, float max);

    std::optional<MotionMsg> m_motion_msg;
    std::optional<EyeMsg> m_eye_msg;
    std::optional<SystemMsg> m_system_msg;
};

#endif // JSON_PROTOCOL_HPP
