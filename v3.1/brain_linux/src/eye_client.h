/**
 * Spider Robot v3.1 - Eye Client
 * 
 * Brain Daemon → Eye Service Unix socket client.
 */

#ifndef EYE_CLIENT_H
#define EYE_CLIENT_H

#include <string>
#include <atomic>

class EyeClient {
public:
    EyeClient();
    ~EyeClient();

    /**
     * Connect to Eye Service (non-blocking, auto-reconnect).
     */
    bool connect();

    /**
     * Disconnect from Eye Service.
     */
    void disconnect();

    /**
     * Check if connected.
     */
    bool isConnected() const { return m_fd >= 0; }

    /**
     * Send raw JSON event.
     */
    bool sendEvent(const std::string& json);

    // High-level API
    bool setMood(const std::string& mood);
    bool lookAt(float x, float y);
    bool blink();
    bool wink(const std::string& eye);
    bool setIrisColor(uint16_t rgb565);
    bool setIdleEnabled(bool enabled);
    bool sendEstop();
    bool requestStatus();

private:
    int m_fd;
    bool m_connect_attempted;
    uint64_t m_last_connect_attempt;
};

#endif // EYE_CLIENT_H
