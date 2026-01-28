/**
 * EyeClientUnix Implementation
 */

#include "eye_client_unix.hpp"

#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

EyeClientUnix::~EyeClientUnix() {
    disconnect();
}

bool EyeClientUnix::connect(const std::string &socket_path) {
    m_socket_path = socket_path;

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        std::cerr << "[EyeClient] Failed to create socket: "
                  << strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "[EyeClient] Failed to connect to " << socket_path
                  << ": " << strerror(errno) << std::endl;
        close(m_fd);
        m_fd = -1;
        return false;
    }

    std::cout << "[EyeClient] Connected to " << socket_path << std::endl;
    return true;
}

bool EyeClientUnix::sendEvent(const std::string &json) {
    if (m_fd < 0) {
        return false;
    }

    // Send length-prefixed JSON
    uint32_t len = json.size();
    if (write(m_fd, &len, sizeof(len)) != sizeof(len)) {
        return false;
    }
    if (write(m_fd, json.c_str(), len) != (ssize_t)len) {
        return false;
    }

    return true;
}

void EyeClientUnix::disconnect() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}
