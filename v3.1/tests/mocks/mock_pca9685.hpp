#ifndef MOCK_PCA9685_HPP
#define MOCK_PCA9685_HPP

#include <cstdint>
#include <vector>
#include <array>

/**
 * MockPCA9685 - Records PWM values for verification
 */
class MockPCA9685 {
public:
    static constexpr int CHANNEL_COUNT = 16;

    struct PwmRecord {
        uint8_t channel;
        uint16_t pulse_us;
        uint64_t timestamp_ms;
    };

    MockPCA9685() {
        reset();
    }

    void reset() {
        m_records.clear();
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            m_current_pwm[i] = 1500;
        }
        m_init_called = false;
        m_sleep_called = false;
    }

    int init(uint8_t addr = 0x40) {
        (void)addr;
        m_init_called = true;
        return 0;
    }

    int setPwmUs(uint8_t channel, uint16_t pulse_us) {
        if (channel >= CHANNEL_COUNT) return -1;

        m_current_pwm[channel] = pulse_us;

        PwmRecord rec;
        rec.channel = channel;
        rec.pulse_us = pulse_us;
        rec.timestamp_ms = m_tick_count;
        m_records.push_back(rec);

        return 0;
    }

    uint16_t getCurrentPwm(uint8_t channel) const {
        if (channel >= CHANNEL_COUNT) return 0;
        return m_current_pwm[channel];
    }

    const std::vector<PwmRecord> &getRecords() const { return m_records; }

    size_t getRecordCount() const { return m_records.size(); }

    void tick() { m_tick_count++; }

    bool wasInitCalled() const { return m_init_called; }
    bool wasSleepCalled() const { return m_sleep_called; }

    void sleep() { m_sleep_called = true; }

private:
    std::array<uint16_t, CHANNEL_COUNT> m_current_pwm;
    std::vector<PwmRecord> m_records;
    uint64_t m_tick_count = 0;
    bool m_init_called = false;
    bool m_sleep_called = false;
};

#endif // MOCK_PCA9685_HPP
