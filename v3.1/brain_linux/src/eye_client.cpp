/**
 * Spider Robot v3.1 - Eye Client Implementation
 */

#include "eye_client.h"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cstdio>

extern "C" {
#include "eye_event_protocol.h"
}

static uint64_t get_time_ms_ec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

EyeClient::EyeClient() : m_fd(-1), m_connect_attempted(false), m_last_connect_attempt(0) {
}

EyeClient::~EyeClient() {
    disconnect();
}

bool EyeClient::connect() {
    if (m_fd >= 0) {
        return true;
    }

    uint64_t now = get_time_ms_ec();
    if (m_connect_attempted && (now - m_last_connect_attempt) < 5000) {
        return false;
    }
    m_connect_attempted = true;
    m_last_connect_attempt = now;

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        std::cerr << "[EyeClient] socket() failed: " << strerror(errno) << std::endl;
        return false;
    }

    int flags = fcntl(m_fd, F_GETFL, 0);
    fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, EYE_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            close(m_fd);
            m_fd = -1;
            return false;
        }
    }

    std::cout << "[EyeClient] Connected to " << EYE_SOCKET_PATH << std::endl;
    return true;
}

void EyeClient::disconnect() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
        std::cout << "[EyeClient] Disconnected" << std::endl;
    }
}

bool EyeClient::sendEvent(const std::string& json) {
    if (m_fd < 0) {
        if (!connect()) {
            return false;
        }
    }

    std::string msg = json + "\n";
    ssize_t n = send(m_fd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (n < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            std::cerr << "[EyeClient] Connection lost" << std::endl;
            disconnect();
        }
        return false;
    }

    return (size_t)n == msg.length();
}

bool EyeClient::setMood(const std::string& mood) {
    char buf[EYE_EVENT_MAX_SIZE];
    snprintf(buf, sizeof(buf), "{\"type\":\"mood\",\"mood\":\"%s\"}", mood.c_str());
    return sendEvent(buf);
}

bool EyeClient::lookAt(float x, float y) {
    char buf[EYE_EVENT_MAX_SIZE];
    snprintf(buf, sizeof(buf), "{\"type\":\"look\",\"x\":%.3f,\"y\":%.3f}", x, y);
    return sendEvent(buf);
}

bool EyeClient::blink() {
    return sendEvent("{\"type\":\"blink\"}");
}

bool EyeClient::wink(const std::string& eye) {
    char buf[EYE_EVENT_MAX_SIZE];
    snprintf(buf, sizeof(buf), "{\"type\":\"wink\",\"eye\":\"%s\"}", eye.c_str());
    return sendEvent(buf);
}

bool EyeClient::setIrisColor(uint16_t rgb565) {
    char buf[EYE_EVENT_MAX_SIZE];
    snprintf(buf, sizeof(buf), "{\"type\":\"color\",\"rgb565\":%u}", rgb565);
    return sendEvent(buf);
}

bool EyeClient::setIdleEnabled(bool enabled) {
    char buf[EYE_EVENT_MAX_SIZE];
    snprintf(buf, sizeof(buf), "{\"type\":\"idle\",\"enabled\":%s}", enabled ? "true" : "false");
    return sendEvent(buf);
}

bool EyeClient::sendEstop() {
    return sendEvent("{\"type\":\"estop\"}");
}

bool EyeClient::requestStatus() {
    return sendEvent("{\"type\":\"status\"}");
}
