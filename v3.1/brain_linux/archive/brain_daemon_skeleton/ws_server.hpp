#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "task_manager.hpp"
#include "error_handler.hpp"

/**
 * WsServer - WebSocket server for Remote/Browser UI
 *
 * Handles JSON protocol v3.1 messages.
 */
class WsServer {
public:
    WsServer(uint16_t port, TaskManager &task_manager, ErrorHandler &error_handler);
    ~WsServer();

    /**
     * Start the WebSocket server.
     */
    bool start();

    /**
     * Stop the WebSocket server.
     */
    void stop();

    /**
     * Poll for events (non-blocking).
     */
    void poll();

    /**
     * Broadcast message to all connected clients.
     */
    void broadcast(const std::string &json);

private:
    void handleMessage(int client_id, const std::string &json);
    void parseMotionCommand(const std::string &json);
    void parseEyeCommand(const std::string &json);

    uint16_t m_port;
    TaskManager &m_task_manager;
    ErrorHandler &m_error_handler;

    // TODO: Actual WebSocket implementation (e.g., libwebsockets, uWebSockets)
    int m_server_fd = -1;
    bool m_running = false;
};

#endif // WS_SERVER_HPP
