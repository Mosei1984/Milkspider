#ifndef ERROR_HANDLER_HPP
#define ERROR_HANDLER_HPP

#include <string>
#include <cstdint>

/**
 * Error severity levels
 */
enum class ErrorLevel {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * ErrorHandler - Centralized error reporting and safety escalation
 *
 * Features:
 * - Rate-limited logging
 * - Safety escalation (auto-ESTOP on critical errors)
 * - Telemetry reporting
 */
class ErrorHandler {
public:
    ErrorHandler();

    /**
     * Report an error.
     * Critical errors trigger safety escalation.
     */
    void report(ErrorLevel level, const std::string &message);

    /**
     * Get error count by level.
     */
    uint32_t getCount(ErrorLevel level) const;

    /**
     * Clear error counts.
     */
    void clearCounts();

    /**
     * Set callback for critical errors (e.g., trigger ESTOP).
     */
    using CriticalCallback = void (*)(const std::string &message);
    void setCriticalCallback(CriticalCallback callback);

private:
    uint32_t m_info_count = 0;
    uint32_t m_warning_count = 0;
    uint32_t m_error_count = 0;
    uint32_t m_critical_count = 0;

    uint64_t m_last_log_time_ms = 0;
    uint32_t m_suppressed_count = 0;

    CriticalCallback m_critical_callback = nullptr;
};

#endif // ERROR_HANDLER_HPP
