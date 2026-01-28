#ifndef EYE_CLIENT_UNIX_HPP
#define EYE_CLIENT_UNIX_HPP

#include <string>

/**
 * EyeClientUnix - Brain → Eye Service communication via Unix socket
 *
 * Sends JSON eye events to the Eye Service.
 */
class EyeClientUnix {
public:
    EyeClientUnix() = default;
    ~EyeClientUnix();

    /**
     * Connect to Eye Service Unix socket.
     */
    bool connect(const std::string &socket_path);

    /**
     * Send JSON event to Eye Service.
     */
    bool sendEvent(const std::string &json);

    /**
     * Disconnect from Eye Service.
     */
    void disconnect();

    /**
     * Check if connected.
     */
    bool isConnected() const { return m_fd >= 0; }

private:
    int m_fd = -1;
    std::string m_socket_path;
};

#endif // EYE_CLIENT_UNIX_HPP
