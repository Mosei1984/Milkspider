#ifndef TELEMETRY_HPP
#define TELEMETRY_HPP

#include <cstdint>

/**
 * Telemetry - System status and metrics
 */
class Telemetry {
public:
    Telemetry();

    /**
     * Called from main loop to update metrics.
     */
    void tick();

    /**
     * Get uptime in seconds.
     */
    uint32_t getUptimeSeconds() const;

    /**
     * Get packets sent count.
     */
    uint32_t getPacketsSent() const { return m_packets_sent; }

    /**
     * Increment packets sent counter.
     */
    void incrementPacketsSent() { m_packets_sent++; }

    /**
     * Get loop frequency (Hz).
     */
    float getLoopHz() const { return m_loop_hz; }

private:
    uint64_t m_start_time_ms = 0;
    uint32_t m_packets_sent = 0;

    uint32_t m_tick_count = 0;
    uint64_t m_last_hz_calc_ms = 0;
    float m_loop_hz = 0.0f;
};

#endif // TELEMETRY_HPP
