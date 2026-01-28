/**
 * WsServer Implementation (Skeleton)
 *
 * TODO: Integrate with actual WebSocket library (libwebsockets or similar)
 */

#include "ws_server.hpp"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

WsServer::WsServer(uint16_t port, TaskManager &task_manager, ErrorHandler &error_handler)
    : m_port(port), m_task_manager(task_manager), m_error_handler(error_handler) {
}

WsServer::~WsServer() {
    stop();
}

bool WsServer::start() {
    // TODO: Replace with actual WebSocket server implementation

    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    if (listen(m_server_fd, 5) < 0) {
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    m_running = true;
    std::cout << "[WsServer] Listening on port " << m_port << std::endl;
    return true;
}

void WsServer::stop() {
    m_running = false;
    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
    }
}

void WsServer::poll() {
    if (!m_running) {
        return;
    }

    // TODO: Poll for WebSocket events (accept, receive, etc.)
    // This is a skeleton - actual implementation needs libwebsockets
}

void WsServer::broadcast(const std::string &json) {
    // TODO: Send to all connected WebSocket clients
    (void)json;
}

void WsServer::handleMessage(int client_id, const std::string &json) {
    (void)client_id;

    // Parse JSON and dispatch to appropriate handler
    // TODO: Use a proper JSON parser (nlohmann/json, rapidjson, etc.)

    if (json.find("\"type\":\"motion\"") != std::string::npos) {
        parseMotionCommand(json);
    } else if (json.find("\"type\":\"eyes\"") != std::string::npos) {
        parseEyeCommand(json);
    } else if (json.find("\"cmd\":\"stop\"") != std::string::npos) {
        m_task_manager.stop();
    } else if (json.find("\"cmd\":\"estop\"") != std::string::npos) {
        m_task_manager.estop();
    }
}

void WsServer::parseMotionCommand(const std::string &json) {
    // TODO: Parse JSON and create MotionCommand
    MotionCommand cmd;
    cmd.name = "forward";  // Placeholder
    m_task_manager.queueMotion(cmd);
}

void WsServer::parseEyeCommand(const std::string &json) {
    // TODO: Forward to Eye Service
    (void)json;
}
