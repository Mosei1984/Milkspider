/**
 * JsonProtocol Implementation
 *
 * Uses simple string parsing. For production, integrate nlohmann/json.
 */

#include "json_protocol.hpp"
#include <sstream>
#include <cstring>
#include <regex>

static std::string extractValue(const std::string &json, const std::string &key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (json[pos] == '"') {
        size_t start = pos + 1;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    } else if (json[pos] == '{') {
        int depth = 1;
        size_t start = pos;
        pos++;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '{') depth++;
            else if (json[pos] == '}') depth--;
            pos++;
        }
        return json.substr(start, pos - start);
    } else {
        size_t start = pos;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
        std::string val = json.substr(start, pos - start);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        return val;
    }
}

static float parseFloat(const std::string &s, float def = 0.0f) {
    try { return std::stof(s); } catch (...) { return def; }
}

static int parseInt(const std::string &s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

JsonProtocol::ParseResult JsonProtocol::parseMessage(const std::string &json) {
    ParseResult result;
    m_motion_msg.reset();
    m_eye_msg.reset();
    m_system_msg.reset();

    if (!validateEnvelope(json)) {
        result.valid = false;
        result.error = "Invalid envelope";
        return result;
    }

    std::string type = extractValue(json, "type");
    result.type = type;

    std::string msg = extractValue(json, "msg");

    if (type == "motion") {
        result.valid = parseMotion(msg);
    } else if (type == "eyes") {
        result.valid = parseEyes(msg);
    } else if (type == "sys") {
        result.valid = parseSystem(msg);
    } else {
        result.valid = false;
        result.error = "Unknown type: " + type;
    }

    return result;
}

bool JsonProtocol::validateEnvelope(const std::string &json) {
    std::string v = extractValue(json, "v");
    if (v != "3.1") return false;

    std::string type = extractValue(json, "type");
    if (type.empty()) return false;

    return true;
}

bool JsonProtocol::parseMotion(const std::string &msg) {
    MotionMsg m;

    m.mode = extractValue(msg, "mode");
    m.cmd = extractValue(msg, "cmd");
    m.interp = extractValue(msg, "interp");
    if (m.interp.empty()) m.interp = "float";

    std::string vec = extractValue(msg, "vec");
    if (!vec.empty()) {
        m.vec_fwd = clampFloat(parseFloat(extractValue(vec, "fwd")), -1.0f, 1.0f);
        m.vec_strafe = clampFloat(parseFloat(extractValue(vec, "strafe")), -1.0f, 1.0f);
        m.vec_turn = clampFloat(parseFloat(extractValue(vec, "turn")), -1.0f, 1.0f);
    }

    std::string stride_s = extractValue(msg, "stride");
    if (!stride_s.empty()) m.stride = clampFloat(parseFloat(stride_s, 1.0f), 0.8f, 1.6f);

    std::string speed_s = extractValue(msg, "speed");
    if (!speed_s.empty()) m.speed = clampFloat(parseFloat(speed_s, 0.8f), 0.0f, 1.0f);

    std::string lift_s = extractValue(msg, "lift");
    if (!lift_s.empty()) m.lift = clampFloat(parseFloat(lift_s, 0.6f), 0.0f, 1.0f);

    m_motion_msg = m;
    return true;
}

bool JsonProtocol::parseEyes(const std::string &msg) {
    EyeMsg e;

    e.cmd = extractValue(msg, "cmd");
    e.mode = extractValue(msg, "mode");
    e.mood = extractValue(msg, "mood");
    e.eye = extractValue(msg, "eye");
    if (e.eye.empty()) e.eye = "both";

    std::string bl_s = extractValue(msg, "bl");
    if (!bl_s.empty()) e.backlight = parseInt(bl_s, 180);
    if (e.backlight < 0) e.backlight = 0;
    if (e.backlight > 255) e.backlight = 255;

    std::string L = extractValue(msg, "L");
    if (!L.empty()) {
        e.lx = clampFloat(parseFloat(extractValue(L, "x")), -1.0f, 1.0f);
        e.ly = clampFloat(parseFloat(extractValue(L, "y")), -1.0f, 1.0f);
    }

    std::string R = extractValue(msg, "R");
    if (!R.empty()) {
        e.rx = clampFloat(parseFloat(extractValue(R, "x")), -1.0f, 1.0f);
        e.ry = clampFloat(parseFloat(extractValue(R, "y")), -1.0f, 1.0f);
    }

    m_eye_msg = e;
    return true;
}

bool JsonProtocol::parseSystem(const std::string &msg) {
    SystemMsg s;
    s.cmd = extractValue(msg, "cmd");
    s.wakepose = extractValue(msg, "wakepose");
    m_system_msg = s;
    return true;
}

float JsonProtocol::clampFloat(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

MotionCommand JsonProtocol::toMotionCommand() const {
    MotionCommand cmd;
    if (m_motion_msg) {
        cmd.name = m_motion_msg->cmd;
        cmd.vec_fwd = m_motion_msg->vec_fwd;
        cmd.vec_strafe = m_motion_msg->vec_strafe;
        cmd.vec_turn = m_motion_msg->vec_turn;
        cmd.stride = m_motion_msg->stride;
        cmd.speed = m_motion_msg->speed;
        cmd.lift = m_motion_msg->lift;
    }
    return cmd;
}

std::string JsonProtocol::createTelemetryResponse(uint32_t uptime_s, float loop_hz,
                                                   uint32_t packets_sent, const std::string &state) {
    std::ostringstream ss;
    ss << "{\"v\":\"3.1\",\"type\":\"telemetry\",\"msg\":{"
       << "\"uptime_s\":" << uptime_s << ","
       << "\"loop_hz\":" << loop_hz << ","
       << "\"packets_sent\":" << packets_sent << ","
       << "\"state\":\"" << state << "\""
       << "}}";
    return ss.str();
}

std::string JsonProtocol::createErrorResponse(int code, const std::string &message) {
    std::ostringstream ss;
    ss << "{\"v\":\"3.1\",\"type\":\"error\",\"msg\":{"
       << "\"code\":" << code << ","
       << "\"message\":\"" << message << "\""
       << "}}";
    return ss.str();
}

std::string JsonProtocol::createAckResponse(const std::string &cmd) {
    std::ostringstream ss;
    ss << "{\"v\":\"3.1\",\"type\":\"ack\",\"msg\":{\"cmd\":\"" << cmd << "\"}}";
    return ss.str();
}
