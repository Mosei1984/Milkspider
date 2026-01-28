#ifndef MOTION_LOADER_HPP
#define MOTION_LOADER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "../../common/limits.h"

struct MotionFrame {
    uint16_t servo_us[SERVO_COUNT_LEGS];  // 8 legacy servos
    uint32_t t_ms;                         // interpolation time
};

struct MotionSequence {
    int id;
    std::string name;
    std::string description;
    std::vector<MotionFrame> frames;
};

class MotionSequenceIterator {
public:
    MotionSequenceIterator(const MotionSequence* seq = nullptr);

    void reset();
    bool hasNext() const;
    const MotionFrame& current() const;
    const MotionFrame& next();
    bool isComplete() const;
    size_t currentIndex() const { return m_index; }
    size_t totalFrames() const;

private:
    const MotionSequence* m_sequence;
    size_t m_index;
};

class MotionLoader {
public:
    bool loadFromFile(const std::string& path);
    
    bool hasSequence(const std::string& name) const;
    const MotionSequence* getSequence(const std::string& name) const;
    const MotionSequence* getSequenceById(int id) const;
    
    MotionSequenceIterator createIterator(const std::string& name) const;
    
    std::vector<std::string> listSequences() const;
    size_t count() const { return m_sequences.size(); }

    static void convertLegacyToPose(const MotionFrame& frame, 
                                    uint16_t* pose_us_13);

private:
    bool parseJson(const std::string& json_content);
    
    std::unordered_map<std::string, MotionSequence> m_sequences;
    std::unordered_map<int, std::string> m_id_to_name;
};

#endif // MOTION_LOADER_HPP
