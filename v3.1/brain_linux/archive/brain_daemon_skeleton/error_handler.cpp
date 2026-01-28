/**
 * ErrorHandler Implementation
 */

#include "error_handler.hpp"

#include <iostream>
#include <chrono>

#define LOG_RATE_LIMIT_MS 100  // Minimum time between logs

static uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

ErrorHandler::ErrorHandler() {
    m_last_log_time_ms = now_ms();
}

void ErrorHandler::report(ErrorLevel level, const std::string &message) {
    // Update counters
    switch (level) {
        case ErrorLevel::INFO:
            m_info_count++;
            break;
        case ErrorLevel::WARNING:
            m_warning_count++;
            break;
        case ErrorLevel::ERROR:
            m_error_count++;
            break;
        case ErrorLevel::CRITICAL:
            m_critical_count++;
            break;
    }

    // Rate limiting
    uint64_t now = now_ms();
    if (now - m_last_log_time_ms < LOG_RATE_LIMIT_MS) {
        m_suppressed_count++;
        // Always log critical errors
        if (level != ErrorLevel::CRITICAL) {
            return;
        }
    }
    m_last_log_time_ms = now;

    // Log message
    const char *level_str = "";
    switch (level) {
        case ErrorLevel::INFO:     level_str = "INFO"; break;
        case ErrorLevel::WARNING:  level_str = "WARN"; break;
        case ErrorLevel::ERROR:    level_str = "ERROR"; break;
        case ErrorLevel::CRITICAL: level_str = "CRITICAL"; break;
    }

    std::cerr << "[" << level_str << "] " << message;
    if (m_suppressed_count > 0) {
        std::cerr << " (+" << m_suppressed_count << " suppressed)";
        m_suppressed_count = 0;
    }
    std::cerr << std::endl;

    // Critical error escalation
    if (level == ErrorLevel::CRITICAL && m_critical_callback) {
        m_critical_callback(message);
    }
}

uint32_t ErrorHandler::getCount(ErrorLevel level) const {
    switch (level) {
        case ErrorLevel::INFO:     return m_info_count;
        case ErrorLevel::WARNING:  return m_warning_count;
        case ErrorLevel::ERROR:    return m_error_count;
        case ErrorLevel::CRITICAL: return m_critical_count;
    }
    return 0;
}

void ErrorHandler::clearCounts() {
    m_info_count = 0;
    m_warning_count = 0;
    m_error_count = 0;
    m_critical_count = 0;
}

void ErrorHandler::setCriticalCallback(CriticalCallback callback) {
    m_critical_callback = callback;
}
