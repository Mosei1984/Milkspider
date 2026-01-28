/**
 * MotionLoader Implementation
 * 
 * Loads motion sequences from JSON and provides frame-by-frame playback.
 * Uses nlohmann/json for parsing (single-header library).
 */

#include "motion_loader.hpp"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// MotionSequenceIterator implementation

MotionSequenceIterator::MotionSequenceIterator(const MotionSequence* seq)
    : m_sequence(seq), m_index(0) {}

void MotionSequenceIterator::reset() {
    m_index = 0;
}

bool MotionSequenceIterator::hasNext() const {
    if (!m_sequence) return false;
    return m_index < m_sequence->frames.size();
}

const MotionFrame& MotionSequenceIterator::current() const {
    static MotionFrame empty = {};
    if (!m_sequence || m_index >= m_sequence->frames.size()) {
        return empty;
    }
    return m_sequence->frames[m_index];
}

const MotionFrame& MotionSequenceIterator::next() {
    const MotionFrame& frame = current();
    if (hasNext()) {
        m_index++;
    }
    return frame;
}

bool MotionSequenceIterator::isComplete() const {
    if (!m_sequence) return true;
    return m_index >= m_sequence->frames.size();
}

size_t MotionSequenceIterator::totalFrames() const {
    if (!m_sequence) return 0;
    return m_sequence->frames.size();
}

// MotionLoader implementation

bool MotionLoader::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseJson(buffer.str());
}

bool MotionLoader::parseJson(const std::string& json_content) {
    try {
        json root = json::parse(json_content);
        
        if (!root.contains("sequences") || !root["sequences"].is_object()) {
            return false;
        }
        
        m_sequences.clear();
        m_id_to_name.clear();
        
        for (auto& [name, seq_data] : root["sequences"].items()) {
            MotionSequence seq;
            seq.name = name;
            seq.id = seq_data.value("id", 0);
            seq.description = seq_data.value("description", "");
            
            if (seq_data.contains("frames") && seq_data["frames"].is_array()) {
                for (auto& frame_data : seq_data["frames"]) {
                    MotionFrame frame = {};
                    
                    if (frame_data.contains("servo_us") && 
                        frame_data["servo_us"].is_array()) {
                        auto& servos = frame_data["servo_us"];
                        for (size_t i = 0; i < SERVO_COUNT_LEGS && i < servos.size(); i++) {
                            frame.servo_us[i] = static_cast<uint16_t>(servos[i].get<int>());
                        }
                    }
                    
                    frame.t_ms = frame_data.value("t_ms", 100);
                    seq.frames.push_back(frame);
                }
            }
            
            m_sequences[name] = seq;
            m_id_to_name[seq.id] = name;
        }
        
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

bool MotionLoader::hasSequence(const std::string& name) const {
    return m_sequences.find(name) != m_sequences.end();
}

const MotionSequence* MotionLoader::getSequence(const std::string& name) const {
    auto it = m_sequences.find(name);
    if (it == m_sequences.end()) {
        return nullptr;
    }
    return &it->second;
}

const MotionSequence* MotionLoader::getSequenceById(int id) const {
    auto it = m_id_to_name.find(id);
    if (it == m_id_to_name.end()) {
        return nullptr;
    }
    return getSequence(it->second);
}

MotionSequenceIterator MotionLoader::createIterator(const std::string& name) const {
    return MotionSequenceIterator(getSequence(name));
}

std::vector<std::string> MotionLoader::listSequences() const {
    std::vector<std::string> names;
    names.reserve(m_sequences.size());
    for (const auto& [name, _] : m_sequences) {
        names.push_back(name);
    }
    return names;
}

void MotionLoader::convertLegacyToPose(const MotionFrame& frame, 
                                        uint16_t* pose_us_13) {
    // Legacy 8-servo format maps directly to CH0-7
    // S0 → CH0 (UR paw)
    // S1 → CH1 (UR arm)
    // S2 → CH2 (LR arm)
    // S3 → CH3 (LR paw)
    // S4 → CH4 (UL paw)
    // S5 → CH5 (UL arm)
    // S6 → CH6 (LL arm)
    // S7 → CH7 (LL paw)
    
    for (int i = 0; i < SERVO_COUNT_LEGS; i++) {
        pose_us_13[i] = clamp_servo_us(frame.servo_us[i]);
    }
    
    // CH8-11: unused, set to neutral
    for (int i = SERVO_COUNT_LEGS; i < SERVO_CHANNEL_SCAN; i++) {
        pose_us_13[i] = SERVO_PWM_NEUTRAL_US;
    }
    
    // CH12: scan servo, keep at neutral (not part of legacy sequences)
    pose_us_13[SERVO_CHANNEL_SCAN] = SERVO_PWM_NEUTRAL_US;
}
