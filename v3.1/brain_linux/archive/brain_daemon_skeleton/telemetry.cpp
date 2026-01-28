/**
 * Telemetry Implementation
 */

#include "telemetry.hpp"

#include <chrono>

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

Telemetry::Telemetry() {
    m_start_time_ms = now_ms();
    m_last_hz_calc_ms = m_start_time_ms;
}

void Telemetry::tick() {
    m_tick_count++;

    uint64_t now = now_ms();
    uint64_t elapsed = now - m_last_hz_calc_ms;

    // Calculate loop Hz every second
    if (elapsed >= 1000) {
        m_loop_hz = (float)m_tick_count * 1000.0f / (float)elapsed;
        m_tick_count = 0;
        m_last_hz_calc_ms = now;
    }
}

uint32_t Telemetry::getUptimeSeconds() const {
    return (uint32_t)((now_ms() - m_start_time_ms) / 1000);
}
