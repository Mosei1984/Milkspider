/**
 * Spider Robot v3.1 - Logger
 * 
 * Simple logging with levels, timestamps, and optional file output.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <string>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void setLevel(LogLevel level) {
        m_level = level;
    }

    void setLevel(const std::string& level) {
        if (level == "DEBUG" || level == "debug") m_level = LogLevel::DEBUG;
        else if (level == "INFO" || level == "info") m_level = LogLevel::INFO;
        else if (level == "WARN" || level == "warn") m_level = LogLevel::WARN;
        else if (level == "ERROR" || level == "error") m_level = LogLevel::ERROR;
    }

    LogLevel getLevel() const { return m_level; }

    bool openFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
        m_file.open(path, std::ios::app);
        if (!m_file.is_open()) {
            std::cerr << "[Logger] Failed to open log file: " << path << std::endl;
            return false;
        }
        m_file_path = path;
        return true;
    }

    void closeFile() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void log(LogLevel level, const char* tag, const char* fmt, ...) {
        if (level < m_level) return;

        char msg[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        char timestamp[32];
        getTimestamp(timestamp, sizeof(timestamp));

        const char* level_str = levelToString(level);

        std::lock_guard<std::mutex> lock(m_mutex);

        std::cout << timestamp << " [" << level_str << "] [" << tag << "] " << msg << std::endl;

        if (m_file.is_open()) {
            m_file << timestamp << " [" << level_str << "] [" << tag << "] " << msg << std::endl;
            m_file.flush();
        }
    }

private:
    Logger() : m_level(LogLevel::INFO) {}
    ~Logger() { closeFile(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void getTimestamp(char* buf, size_t size) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm_info;
        localtime_r(&ts.tv_sec, &tm_info);
        int ms = ts.tv_nsec / 1000000;
        snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms);
    }

    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "?????";
        }
    }

    LogLevel m_level;
    std::ofstream m_file;
    std::string m_file_path;
    std::mutex m_mutex;
};

#define LOG_DEBUG(tag, fmt, ...) Logger::instance().log(LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)  Logger::instance().log(LogLevel::INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  Logger::instance().log(LogLevel::WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) Logger::instance().log(LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
