/**
 * Spider Robot v3.1 - Brain Daemon
 * 
 * Main entry point for Brain core running on Milk-V Duo Linux side.
 * 
 * Responsibilities:
 * - WebSocket server (port 9000) for external control
 * - Mailbox communication via /dev/cvi-rtos-cmdqu
 * - Shared memory ring buffer at 0x83F00000 for PosePacket31
 * - Send motion packets to FreeRTOS Muscle Runtime
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <getopt.h>

#include "mailbox.h"
#include "shared_memory.h"
#include "eye_client.h"
#include "distance_sensor.h"
#include "serial_control.h"
#include "scan_controller.h"
#include "logger.h"

extern "C" {
#include "protocol_posepacket31.h"
#include "crc16_ccitt_false.h"
#include "eye_event_protocol.h"
}

#define WS_PORT                   9000
#define HEARTBEAT_INTERVAL_MS     100
#define MAX_CLIENTS               8
#define RX_BUFFER_SIZE            4096
#define EYE_RECONNECT_INTERVAL_MS 5000
#define WATCHDOG_LOG_INTERVAL_MS  10000
#define STATS_LOG_INTERVAL_MS     30000
#define DEFAULT_SERIAL_PORT       "/dev/ttyS0"
#define DEFAULT_SERIAL_BAUD       115200

static std::atomic<bool> g_shutdown{false};
static std::atomic<bool> g_estop{false};
static std::atomic<bool> g_estop_prev{false};

void signal_handler(int sig) {
    (void)sig;
    g_shutdown.store(true);
}

struct WsClient {
    int fd = -1;
    bool handshake_done = false;
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t rx_len = 0;
};

class BrainDaemon {
public:
    bool init();
    void run();
    void shutdown();
    
    void setSerialPort(const std::string& port) { m_serial_port = port; }
    void setSerialBaud(int baud) { m_serial_baud = baud; }

private:
    bool initWebSocket();
    bool initSerialControl();
    void acceptClients();
    void processClients();
    void removeClient(int idx);
    
    bool wsHandshake(WsClient& client);
    void wsProcessFrame(WsClient& client);
    void wsSendFrame(int fd, const uint8_t* data, size_t len, uint8_t opcode = 0x01);
    void wsBroadcast(const std::string& msg);
    
    void handleCommand(const std::string& cmd);
    void sendPosePacket(const PosePacket31& pkt);
    void tickHeartbeat();
    void tickEyeReconnect();
    void tickWatchdogLog();
    void tickStatsLog();
    void checkEstopStateChange();
    
    PosePacket31 buildPosePacket(uint32_t t_ms, uint16_t flags, const uint16_t* servos);
    
    void handleEyeCommand(const std::string& cmd);
    void handleDistanceCommand(const std::string& cmd);
    void handleScanCommand(const std::string& cmd);
    
    void initScanController();
    void setScanServoAngle(int angle_deg);
    
    void onSerialServo(int channel, uint16_t us);
    void onSerialServos(const uint16_t* us, int count);
    void onSerialMove(uint32_t t_ms, const uint16_t* us, int count);
    void onSerialScan(uint16_t us);
    void onSerialEstop();
    void onSerialResume();
    std::string onSerialStatus();
    int onSerialDistance();
    
    Mailbox m_mailbox;
    SharedMemory m_shared_mem;
    EyeClient m_eye_client;
    DistanceSensor m_distance_sensor;
    SerialControl m_serial_control;
    ScanController m_scan_controller;
    
    std::string m_serial_port = DEFAULT_SERIAL_PORT;
    int m_serial_baud = DEFAULT_SERIAL_BAUD;
    
    int m_server_fd = -1;
    std::vector<WsClient> m_clients;
    
    uint32_t m_seq = 0;
    uint64_t m_last_heartbeat_ms = 0;
    uint64_t m_last_eye_reconnect_ms = 0;
    uint64_t m_last_watchdog_log_ms = 0;
    uint64_t m_last_stats_log_ms = 0;
    uint64_t m_start_time_ms = 0;
    
    uint32_t m_packets_sent = 0;
    bool m_eye_connected = false;
    bool m_distance_available = false;
    bool m_serial_available = false;
    
    uint16_t m_current_servos[SERVO_COUNT_TOTAL];
};

static uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool BrainDaemon::init() {
    m_start_time_ms = get_time_ms();
    LOG_INFO("Brain", "Spider Robot v3.1 Brain Daemon starting...");
    
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        m_current_servos[i] = SERVO_PWM_NEUTRAL_US;
    }
    
    if (!m_mailbox.open()) {
        LOG_ERROR("Brain", "CRITICAL: Failed to open mailbox");
        return false;
    }
    
    if (!m_shared_mem.map()) {
        LOG_ERROR("Brain", "CRITICAL: Failed to map shared memory");
        return false;
    }
    
    if (!initWebSocket()) {
        LOG_ERROR("Brain", "CRITICAL: Failed to start WebSocket server");
        return false;
    }
    
    if (m_eye_client.connect()) {
        m_eye_connected = true;
        LOG_INFO("Brain", "Eye Service connected");
    } else {
        m_eye_connected = false;
        LOG_WARN("Brain", "Eye Service not available (will retry every %d ms)", EYE_RECONNECT_INTERVAL_MS);
    }
    
    if (m_distance_sensor.init()) {
        m_distance_available = true;
        LOG_INFO("Brain", "Distance sensor initialized");
    } else {
        m_distance_available = false;
        LOG_WARN("Brain", "Distance sensor not available - continuing without it");
    }
    
    if (initSerialControl()) {
        m_serial_available = true;
        LOG_INFO("Brain", "Serial control initialized on %s", m_serial_port.c_str());
    } else {
        m_serial_available = false;
        LOG_WARN("Brain", "Serial control not available - continuing without it");
    }
    
    // Initialize scan controller
    initScanController();
    
    LOG_INFO("Brain", "All systems initialized");
    return true;
}

bool BrainDaemon::initWebSocket() {
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        LOG_ERROR("WS", "socket() failed: %s", strerror(errno));
        return false;
    }
    
    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    int flags = fcntl(m_server_fd, F_GETFL, 0);
    fcntl(m_server_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WS_PORT);
    
    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("WS", "bind() failed: %s", strerror(errno));
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }
    
    if (listen(m_server_fd, 5) < 0) {
        LOG_ERROR("WS", "listen() failed: %s", strerror(errno));
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }
    
    LOG_INFO("WS", "Listening on port %d", WS_PORT);
    return true;
}

bool BrainDaemon::initSerialControl() {
    m_serial_control.setPort(m_serial_port);
    m_serial_control.setBaudRate(m_serial_baud);
    m_serial_control.setEyeClient(&m_eye_client);
    
    m_serial_control.setServoCallback([this](int ch, uint16_t us) { onSerialServo(ch, us); });
    m_serial_control.setServosCallback([this](const uint16_t* us, int count) { onSerialServos(us, count); });
    m_serial_control.setMoveCallback([this](uint32_t t, const uint16_t* us, int count) { onSerialMove(t, us, count); });
    m_serial_control.setScanCallback([this](uint16_t us) { onSerialScan(us); });
    m_serial_control.setEstopCallback([this]() { onSerialEstop(); });
    m_serial_control.setResumeCallback([this]() { onSerialResume(); });
    m_serial_control.setStatusCallback([this]() { return onSerialStatus(); });
    m_serial_control.setDistanceCallback([this]() { return onSerialDistance(); });
    
    return m_serial_control.init();
}

void BrainDaemon::run() {
    uint64_t now = get_time_ms();
    m_last_heartbeat_ms = now;
    m_last_eye_reconnect_ms = now;
    m_last_watchdog_log_ms = now;
    m_last_stats_log_ms = now;
    
    while (!g_shutdown.load()) {
        acceptClients();
        processClients();
        m_serial_control.tick();
        m_scan_controller.tick();
        tickHeartbeat();
        tickEyeReconnect();
        tickWatchdogLog();
        tickStatsLog();
        checkEstopStateChange();
        
        usleep(1000);
    }
}

void BrainDaemon::acceptClients() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(m_server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        return;
    }
    
    if (m_clients.size() >= MAX_CLIENTS) {
        LOG_WARN("WS", "Max clients (%d) reached, rejecting connection", MAX_CLIENTS);
        close(client_fd);
        return;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    WsClient client;
    client.fd = client_fd;
    client.handshake_done = false;
    client.rx_len = 0;
    m_clients.push_back(client);
    
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    LOG_INFO("WS", "Client connected from %s (total: %zu)", ip, m_clients.size());
}

void BrainDaemon::processClients() {
    for (size_t i = 0; i < m_clients.size(); ) {
        WsClient& client = m_clients[i];
        
        ssize_t n = recv(client.fd, client.rx_buffer + client.rx_len,
                         sizeof(client.rx_buffer) - client.rx_len, 0);
        
        if (n > 0) {
            client.rx_len += n;
            
            if (!client.handshake_done) {
                if (!wsHandshake(client)) {
                    removeClient(i);
                    continue;
                }
            } else {
                wsProcessFrame(client);
            }
        } else if (n == 0) {
            LOG_INFO("WS", "Client disconnected (remaining: %zu)", m_clients.size() - 1);
            removeClient(i);
            continue;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("WS", "recv error: %s", strerror(errno));
            removeClient(i);
            continue;
        }
        
        i++;
    }
}

void BrainDaemon::removeClient(int idx) {
    close(m_clients[idx].fd);
    m_clients.erase(m_clients.begin() + idx);
}

static const char* ws_find_header(const char* headers, const char* name) {
    const char* p = strstr(headers, name);
    if (!p) return nullptr;
    p += strlen(name);
    while (*p == ' ' || *p == ':') p++;
    return p;
}

static void base64_encode(const uint8_t* in, size_t len, char* out) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, j;
    for (i = 0, j = 0; i < len; i += 3, j += 4) {
        uint32_t v = in[i] << 16;
        if (i + 1 < len) v |= in[i + 1] << 8;
        if (i + 2 < len) v |= in[i + 2];
        out[j]     = b64[(v >> 18) & 0x3F];
        out[j + 1] = b64[(v >> 12) & 0x3F];
        out[j + 2] = (i + 1 < len) ? b64[(v >> 6) & 0x3F] : '=';
        out[j + 3] = (i + 2 < len) ? b64[v & 0x3F] : '=';
    }
    out[j] = '\0';
}

#include "sha1.h"

bool BrainDaemon::wsHandshake(WsClient& client) {
    const char* end = strstr((char*)client.rx_buffer, "\r\n\r\n");
    if (!end) {
        return true;
    }
    
    const char* key_header = ws_find_header((char*)client.rx_buffer, "Sec-WebSocket-Key");
    if (!key_header) {
        LOG_ERROR("WS", "Missing Sec-WebSocket-Key in handshake");
        return false;
    }
    
    char key[64] = {};
    const char* key_end = strpbrk(key_header, "\r\n");
    size_t key_len = key_end ? (key_end - key_header) : strlen(key_header);
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    strncpy(key, key_header, key_len);
    
    const char* ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat[128];
    snprintf(concat, sizeof(concat), "%s%s", key, ws_guid);
    
    uint8_t sha1_hash[20];
    sha1((uint8_t*)concat, strlen(concat), sha1_hash);
    
    char accept[32];
    base64_encode(sha1_hash, 20, accept);
    
    char response[256];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept);
    
    send(client.fd, response, strlen(response), 0);
    
    size_t consumed = (end - (char*)client.rx_buffer) + 4;
    if (consumed < client.rx_len) {
        memmove(client.rx_buffer, client.rx_buffer + consumed, client.rx_len - consumed);
        client.rx_len -= consumed;
    } else {
        client.rx_len = 0;
    }
    
    client.handshake_done = true;
    LOG_DEBUG("WS", "Handshake complete");
    
    wsSendFrame(client.fd, (const uint8_t*)"{\"status\":\"connected\",\"version\":\"3.1\"}", 38);
    
    return true;
}

void BrainDaemon::wsProcessFrame(WsClient& client) {
    while (client.rx_len >= 2) {
        uint8_t* buf = client.rx_buffer;
        
        bool fin = buf[0] & 0x80;
        uint8_t opcode = buf[0] & 0x0F;
        bool masked = buf[1] & 0x80;
        uint64_t payload_len = buf[1] & 0x7F;
        
        size_t header_len = 2;
        if (payload_len == 126) {
            if (client.rx_len < 4) return;
            payload_len = (buf[2] << 8) | buf[3];
            header_len = 4;
        } else if (payload_len == 127) {
            if (client.rx_len < 10) return;
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | buf[2 + i];
            }
            header_len = 10;
        }
        
        if (masked) header_len += 4;
        
        if (client.rx_len < header_len + payload_len) {
            return;
        }
        
        uint8_t* payload = buf + header_len;
        if (masked) {
            uint8_t* mask = buf + header_len - 4;
            for (size_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask[i % 4];
            }
        }
        
        if (opcode == 0x01 && fin) {
            std::string msg((char*)payload, payload_len);
            handleCommand(msg);
        } else if (opcode == 0x08) {
            LOG_DEBUG("WS", "Client sent close frame");
            wsSendFrame(client.fd, nullptr, 0, 0x08);
        } else if (opcode == 0x09) {
            wsSendFrame(client.fd, payload, payload_len, 0x0A);
        }
        
        size_t frame_len = header_len + payload_len;
        if (frame_len < client.rx_len) {
            memmove(client.rx_buffer, client.rx_buffer + frame_len, client.rx_len - frame_len);
            client.rx_len -= frame_len;
        } else {
            client.rx_len = 0;
        }
    }
}

void BrainDaemon::wsSendFrame(int fd, const uint8_t* data, size_t len, uint8_t opcode) {
    uint8_t header[10];
    size_t header_len;
    
    header[0] = 0x80 | opcode;
    
    if (len < 126) {
        header[1] = len;
        header_len = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (len >> ((7 - i) * 8)) & 0xFF;
        }
        header_len = 10;
    }
    
    send(fd, header, header_len, 0);
    if (len > 0 && data) {
        send(fd, data, len, 0);
    }
}

void BrainDaemon::wsBroadcast(const std::string& msg) {
    for (auto& client : m_clients) {
        if (client.handshake_done) {
            wsSendFrame(client.fd, (const uint8_t*)msg.c_str(), msg.size());
        }
    }
}

static int parseJsonInt(const std::string& json, const char* key, int default_val) {
    std::string search = std::string("\"") + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return default_val;
    bool neg = false;
    if (json[pos] == '-') { neg = true; pos++; }
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return neg ? -val : val;
}

static int parseJsonIntArray(const std::string& json, const char* key, uint16_t* out, int max_count) {
    std::string search = std::string("\"") + key + "\":[";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = std::string("\"") + key + "\": [";
        pos = json.find(search);
        if (pos == std::string::npos) return 0;
    }
    pos = json.find('[', pos);
    if (pos == std::string::npos) return 0;
    pos++;
    
    int count = 0;
    while (pos < json.size() && count < max_count) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ',')) pos++;
        if (pos >= json.size() || json[pos] == ']') break;
        int val = 0;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
            val = val * 10 + (json[pos] - '0');
            pos++;
        }
        out[count++] = (uint16_t)val;
    }
    return count;
}

static bool parseJsonString(const std::string& json, const char* key, char* out, size_t out_size) {
    std::string search = std::string("\"") + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = std::string("\"") + key + "\": \"";
        pos = json.find(search);
        if (pos == std::string::npos) return false;
    }
    pos = json.find('"', pos + strlen(key) + 2);
    if (pos == std::string::npos) return false;
    pos++;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return false;
    size_t len = std::min(end - pos, out_size - 1);
    memcpy(out, json.c_str() + pos, len);
    out[len] = '\0';
    return true;
}

static int servoNameToChannel(const char* name) {
    static const char* names[] = {
        "leg0_coxa", "leg0_femur",
        "leg1_coxa", "leg1_femur",
        "leg2_coxa", "leg2_femur",
        "leg3_coxa", "leg3_femur",
        "aux0", "aux1", "aux2", "aux3",
        "scan"
    };
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        if (strcmp(name, names[i]) == 0) return i;
    }
    return -1;
}

// Helper to check if JSON contains a type field with given value
static bool hasType(const std::string& json, const char* type_value) {
    // Match both "type":"value" and "type": "value" (with optional space)
    std::string pattern1 = std::string("\"type\":\"") + type_value + "\"";
    std::string pattern2 = std::string("\"type\": \"") + type_value + "\"";
    return json.find(pattern1) != std::string::npos || 
           json.find(pattern2) != std::string::npos;
}

static bool hasCmd(const std::string& json, const char* cmd_value) {
    std::string pattern1 = std::string("\"cmd\":\"") + cmd_value + "\"";
    std::string pattern2 = std::string("\"cmd\": \"") + cmd_value + "\"";
    return json.find(pattern1) != std::string::npos || 
           json.find(pattern2) != std::string::npos;
}

void BrainDaemon::handleCommand(const std::string& cmd) {
    LOG_DEBUG("Brain", "Received: %s", cmd.c_str());
    
    if (hasCmd(cmd, "estop") || hasType(cmd, "estop")) {
        g_estop.store(true);
        m_mailbox.sendEstop();
        wsBroadcast("{\"status\":\"estop_activated\"}");
        return;
    }
    
    if (hasCmd(cmd, "stop") || hasType(cmd, "stop")) {
        g_estop.store(false);
        wsBroadcast("{\"status\":\"stopped\"}");
        return;
    }
    
    if (hasCmd(cmd, "resume") || hasCmd(cmd, "clear_estop") || hasType(cmd, "resume")) {
        g_estop.store(false);
        wsBroadcast("{\"status\":\"resumed\"}");
        return;
    }
    
    if (hasType(cmd, "pose")) {
        uint16_t flags = FLAG_CLAMP_ENABLE;
        if (g_estop.load()) flags |= FLAG_ESTOP;
        
        PosePacket31 pkt = buildPosePacket(100, flags, m_current_servos);
        sendPosePacket(pkt);
        
        wsBroadcast("{\"status\":\"pose_sent\",\"seq\":" + std::to_string(pkt.seq) + "}");
        return;
    }
    
    if (hasType(cmd, "status") || hasCmd(cmd, "status")) {
        char status[256];
        snprintf(status, sizeof(status),
            "{\"status\":\"ok\",\"seq\":%u,\"tx_count\":%u,\"ring_w\":%u,\"ring_r\":%u,\"clients\":%zu}",
            m_seq, m_mailbox.getTxCount(),
            m_shared_mem.getWriteIdx(), m_shared_mem.getReadIdx(),
            m_clients.size());
        wsBroadcast(status);
        return;
    }
    
    if (hasType(cmd, "servo")) {
        int channel = -1;
        char name[32] = {0};
        if (parseJsonString(cmd, "name", name, sizeof(name))) {
            channel = servoNameToChannel(name);
        } else {
            channel = parseJsonInt(cmd, "channel", -1);
        }
        
        if (channel < 0 || channel >= SERVO_COUNT_TOTAL) {
            wsBroadcast("{\"error\":\"invalid_channel\"}");
            return;
        }
        
        int us = parseJsonInt(cmd, "us", -1);
        if (us < 0) {
            wsBroadcast("{\"error\":\"missing_us\"}");
            return;
        }
        
        uint16_t clamped = clamp_servo_us((uint16_t)us);
        m_current_servos[channel] = clamped;
        
        uint16_t flags = FLAG_CLAMP_ENABLE;
        if (g_estop.load()) flags |= FLAG_ESTOP;
        PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
        sendPosePacket(pkt);
        
        char resp[64];
        snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"channel\":%d,\"us\":%u}", channel, clamped);
        wsBroadcast(resp);
        return;
    }
    
    if (hasType(cmd, "servos")) {
        uint16_t values[SERVO_COUNT_TOTAL];
        int count = parseJsonIntArray(cmd, "us", values, SERVO_COUNT_TOTAL);
        
        if (count != SERVO_COUNT_TOTAL) {
            char err[64];
            snprintf(err, sizeof(err), "{\"error\":\"expected_%d_servos\",\"got\":%d}", SERVO_COUNT_TOTAL, count);
            wsBroadcast(err);
            return;
        }
        
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            m_current_servos[i] = clamp_servo_us(values[i]);
        }
        
        uint16_t flags = FLAG_CLAMP_ENABLE;
        if (g_estop.load()) flags |= FLAG_ESTOP;
        PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
        sendPosePacket(pkt);
        
        char resp[48];
        snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"count\":%d}", SERVO_COUNT_TOTAL);
        wsBroadcast(resp);
        return;
    }
    
    if (hasType(cmd, "get_servos")) {
        std::string resp = "{\"servos\":[";
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            if (i > 0) resp += ",";
            resp += std::to_string(m_current_servos[i]);
        }
        resp += "]}";
        wsBroadcast(resp);
        return;
    }
    
    if (hasType(cmd, "move")) {
        int t_ms = parseJsonInt(cmd, "t_ms", 0);
        if (t_ms < 0) t_ms = 0;
        
        uint16_t values[SERVO_COUNT_TOTAL];
        int count = parseJsonIntArray(cmd, "us", values, SERVO_COUNT_TOTAL);
        
        if (count != SERVO_COUNT_TOTAL) {
            char err[64];
            snprintf(err, sizeof(err), "{\"error\":\"expected_%d_servos\",\"got\":%d}", SERVO_COUNT_TOTAL, count);
            wsBroadcast(err);
            return;
        }
        
        for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
            m_current_servos[i] = clamp_servo_us(values[i]);
        }
        
        uint16_t flags = FLAG_CLAMP_ENABLE;
        if (g_estop.load()) flags |= FLAG_ESTOP;
        PosePacket31 pkt = buildPosePacket((uint32_t)t_ms, flags, m_current_servos);
        sendPosePacket(pkt);
        
        char resp[64];
        snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"t_ms\":%d,\"seq\":%u}", t_ms, pkt.seq);
        wsBroadcast(resp);
        return;
    }
    
    // Eye commands - forward to Eye Service
    if (hasType(cmd, "eye") || hasType(cmd, "look") || hasType(cmd, "blink") ||
        hasType(cmd, "wink") || hasType(cmd, "mood")) {
        handleEyeCommand(cmd);
        return;
    }
    
    // Distance sensor command
    if (hasType(cmd, "distance")) {
        handleDistanceCommand(cmd);
        return;
    }
    
    // Scan servo manual command (CH12): {"type":"scan","us":1500}
    if (hasType(cmd, "scan")) {
        int us = parseJsonInt(cmd, "us", -1);
        if (us < 0) {
            wsBroadcast("{\"error\":\"missing_us\"}");
            return;
        }
        
        uint16_t clamped = clamp_servo_us((uint16_t)us);
        m_current_servos[SERVO_CHANNEL_SCAN] = clamped;
        
        uint16_t flags = FLAG_CLAMP_ENABLE | FLAG_SCAN_ENABLE;
        if (g_estop.load()) flags |= FLAG_ESTOP;
        PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
        sendPosePacket(pkt);
        
        char resp[64];
        snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"scan_us\":%u}", clamped);
        wsBroadcast(resp);
        return;
    }
    
    // Autonomous scan controller commands
    if (hasType(cmd, "scan_start") || hasType(cmd, "scan_stop") ||
        hasType(cmd, "scan_status") || hasType(cmd, "scan_get_data")) {
        handleScanCommand(cmd);
        return;
    }
    
    wsBroadcast("{\"error\":\"unknown_command\"}");
}

void BrainDaemon::handleEyeCommand(const std::string& cmd) {
    auto eyeCommandFailed = [this]() {
        if (m_eye_connected) {
            m_eye_connected = false;
            LOG_WARN("Eye", "Eye Service disconnected, will attempt reconnection");
        }
        wsBroadcast("{\"error\":\"eye_service_unavailable\"}");
    };
    
    // look: {"type":"look","x":0.0,"y":0.0} or {"type":"eye","look":{"x":0.0,"y":0.0}}
    if (hasType(cmd, "look")) {
        float x = 0.0f, y = 0.0f;
        const char* px = strstr(cmd.c_str(), "\"x\":");
        const char* py = strstr(cmd.c_str(), "\"y\":");
        if (px) x = strtof(px + 4, nullptr);
        if (py) y = strtof(py + 4, nullptr);
        
        if (m_eye_client.lookAt(x, y)) {
            wsBroadcast("{\"status\":\"ok\",\"eye\":\"look\"}");
        } else {
            eyeCommandFailed();
        }
        return;
    }
    
    // blink: {"type":"blink"}
    if (hasType(cmd, "blink")) {
        if (m_eye_client.blink()) {
            wsBroadcast("{\"status\":\"ok\",\"eye\":\"blink\"}");
        } else {
            eyeCommandFailed();
        }
        return;
    }
    
    // wink: {"type":"wink","eye":"left|right"}
    if (hasType(cmd, "wink")) {
        std::string eye = "left";
        if (cmd.find("\"eye\":\"right\"") != std::string::npos) {
            eye = "right";
        }
        if (m_eye_client.wink(eye)) {
            wsBroadcast("{\"status\":\"ok\",\"eye\":\"wink\",\"which\":\"" + eye + "\"}");
        } else {
            eyeCommandFailed();
        }
        return;
    }
    
    // mood: {"type":"mood","mood":"normal|angry|happy|sleepy"}
    if (hasType(cmd, "mood")) {
        std::string mood = "normal";
        if (cmd.find("\"mood\":\"angry\"") != std::string::npos) mood = "angry";
        else if (cmd.find("\"mood\":\"happy\"") != std::string::npos) mood = "happy";
        else if (cmd.find("\"mood\":\"sleepy\"") != std::string::npos) mood = "sleepy";
        
        if (m_eye_client.setMood(mood)) {
            wsBroadcast("{\"status\":\"ok\",\"eye\":\"mood\",\"mood\":\"" + mood + "\"}");
        } else {
            eyeCommandFailed();
        }
        return;
    }
    
    // generic eye command: {"type":"eye","action":"..."}
    if (hasType(cmd, "eye")) {
        if (cmd.find("\"action\":\"idle_on\"") != std::string::npos) {
            if (m_eye_client.setIdleEnabled(true)) {
                wsBroadcast("{\"status\":\"ok\",\"eye\":\"idle_on\"}");
            } else {
                eyeCommandFailed();
            }
        } else if (cmd.find("\"action\":\"idle_off\"") != std::string::npos) {
            if (m_eye_client.setIdleEnabled(false)) {
                wsBroadcast("{\"status\":\"ok\",\"eye\":\"idle_off\"}");
            } else {
                eyeCommandFailed();
            }
        } else if (cmd.find("\"action\":\"status\"") != std::string::npos) {
            if (m_eye_client.requestStatus()) {
                wsBroadcast("{\"status\":\"ok\",\"eye\":\"status_requested\"}");
            } else {
                eyeCommandFailed();
            }
        } else {
            wsBroadcast("{\"error\":\"unknown_eye_action\"}");
        }
        return;
    }
    
    wsBroadcast("{\"error\":\"unknown_eye_command\"}");
}

void BrainDaemon::handleDistanceCommand(const std::string& cmd) {
    (void)cmd;
    
    if (!m_distance_available) {
        LOG_DEBUG("Distance", "Distance command received but sensor unavailable");
        wsBroadcast("{\"error\":\"distance_sensor_not_available\"}");
        return;
    }
    
    uint16_t distance_mm = 0;
    DistanceSensor::Status status = m_distance_sensor.readRange(distance_mm);
    
    char resp[128];
    switch (status) {
    case DistanceSensor::Status::OK:
        snprintf(resp, sizeof(resp), 
            "{\"type\":\"distance\",\"distance_mm\":%u,\"status\":\"ok\"}", 
            distance_mm);
        break;
    case DistanceSensor::Status::TIMEOUT:
        LOG_DEBUG("Distance", "Read timeout");
        snprintf(resp, sizeof(resp), 
            "{\"type\":\"distance\",\"status\":\"timeout\"}");
        break;
    case DistanceSensor::Status::OUT_OF_RANGE:
        snprintf(resp, sizeof(resp), 
            "{\"type\":\"distance\",\"status\":\"out_of_range\",\"distance_mm\":%u}", 
            distance_mm);
        break;
    default:
        LOG_WARN("Distance", "Read error");
        snprintf(resp, sizeof(resp), 
            "{\"type\":\"distance\",\"status\":\"error\"}");
        break;
    }
    
    wsBroadcast(resp);
}

PosePacket31 BrainDaemon::buildPosePacket(uint32_t t_ms, uint16_t flags, const uint16_t* servos) {
    PosePacket31 pkt;
    pkt.magic = SPIDER_MAGIC;
    pkt.ver_major = SPIDER_VERSION_MAJOR;
    pkt.ver_minor = SPIDER_VERSION_MINOR;
    pkt.seq = ++m_seq;
    pkt.t_ms = t_ms;
    pkt.flags = flags;
    
    for (int i = 0; i < SERVO_COUNT_TOTAL; i++) {
        pkt.servo_us[i] = clamp_servo_us(servos[i]);
    }
    
    pkt.crc16 = crc16_ccitt_false((const uint8_t*)&pkt, sizeof(pkt) - 2);
    
    return pkt;
}

void BrainDaemon::sendPosePacket(const PosePacket31& pkt) {
    if (g_estop.load() && !(pkt.flags & FLAG_ESTOP)) {
        LOG_DEBUG("Brain", "E-STOP active, dropping packet seq=%u", pkt.seq);
        return;
    }
    
    uint32_t write_idx;
    if (!m_shared_mem.writePacket(&pkt, write_idx)) {
        LOG_ERROR("Brain", "Failed to write to shared memory");
        return;
    }
    
    if (!m_mailbox.notifyPacketReady(write_idx)) {
        LOG_ERROR("Brain", "Failed to notify via mailbox");
        return;
    }
    
    m_packets_sent++;
}

void BrainDaemon::tickHeartbeat() {
    uint64_t now = get_time_ms();
    if (now - m_last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
        m_mailbox.sendHeartbeat();
        m_last_heartbeat_ms = now;
    }
}

void BrainDaemon::tickEyeReconnect() {
    if (m_eye_connected) return;
    
    uint64_t now = get_time_ms();
    if (now - m_last_eye_reconnect_ms >= EYE_RECONNECT_INTERVAL_MS) {
        m_last_eye_reconnect_ms = now;
        
        if (m_eye_client.connect()) {
            m_eye_connected = true;
            LOG_INFO("Eye", "Reconnected to Eye Service");
        } else {
            LOG_INFO("Eye", "Reconnection attempt failed, will retry");
        }
    }
}

void BrainDaemon::tickWatchdogLog() {
    uint64_t now = get_time_ms();
    if (now - m_last_watchdog_log_ms >= WATCHDOG_LOG_INTERVAL_MS) {
        m_last_watchdog_log_ms = now;
        
        uint32_t heartbeats = m_mailbox.getTxCount();
        LOG_DEBUG("Watchdog", "Heartbeat tx_count=%u, estop=%s", 
            heartbeats, g_estop.load() ? "ACTIVE" : "clear");
    }
}

void BrainDaemon::tickStatsLog() {
    uint64_t now = get_time_ms();
    if (now - m_last_stats_log_ms >= STATS_LOG_INTERVAL_MS) {
        m_last_stats_log_ms = now;
        
        uint64_t uptime_s = (now - m_start_time_ms) / 1000;
        uint32_t uptime_h = uptime_s / 3600;
        uint32_t uptime_m = (uptime_s % 3600) / 60;
        uint32_t uptime_sec = uptime_s % 60;
        
        LOG_INFO("Stats", "uptime=%02u:%02u:%02u packets_sent=%u clients=%zu eye=%s dist=%s serial=%s",
            uptime_h, uptime_m, uptime_sec,
            m_packets_sent, m_clients.size(),
            m_eye_connected ? "connected" : "disconnected",
            m_distance_available ? "available" : "unavailable",
            m_serial_available ? "available" : "unavailable");
    }
}

void BrainDaemon::checkEstopStateChange() {
    bool current = g_estop.load();
    bool prev = g_estop_prev.load();
    
    if (current != prev) {
        g_estop_prev.store(current);
        if (current) {
            LOG_WARN("ESTOP", "Emergency stop TRIGGERED");
        } else {
            LOG_INFO("ESTOP", "Emergency stop CLEARED");
        }
    }
}

void BrainDaemon::shutdown() {
    LOG_INFO("Brain", "Shutting down...");
    
    for (auto& client : m_clients) {
        wsSendFrame(client.fd, nullptr, 0, 0x08);
        close(client.fd);
    }
    m_clients.clear();
    
    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
    }
    
    m_serial_control.shutdown();
    m_eye_client.disconnect();
    m_shared_mem.unmap();
    m_mailbox.close();
    
    LOG_INFO("Brain", "Shutdown complete");
}

void BrainDaemon::onSerialServo(int channel, uint16_t us) {
    m_current_servos[channel] = clamp_servo_us(us);
    
    uint16_t flags = FLAG_CLAMP_ENABLE;
    if (g_estop.load()) flags |= FLAG_ESTOP;
    PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
    sendPosePacket(pkt);
}

void BrainDaemon::onSerialServos(const uint16_t* us, int count) {
    for (int i = 0; i < count && i < SERVO_COUNT_TOTAL; i++) {
        m_current_servos[i] = clamp_servo_us(us[i]);
    }
    
    uint16_t flags = FLAG_CLAMP_ENABLE;
    if (g_estop.load()) flags |= FLAG_ESTOP;
    PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
    sendPosePacket(pkt);
}

void BrainDaemon::onSerialMove(uint32_t t_ms, const uint16_t* us, int count) {
    for (int i = 0; i < count && i < SERVO_COUNT_TOTAL; i++) {
        m_current_servos[i] = clamp_servo_us(us[i]);
    }
    
    uint16_t flags = FLAG_CLAMP_ENABLE;
    if (g_estop.load()) flags |= FLAG_ESTOP;
    PosePacket31 pkt = buildPosePacket(t_ms, flags, m_current_servos);
    sendPosePacket(pkt);
}

void BrainDaemon::onSerialScan(uint16_t us) {
    m_current_servos[SERVO_CHANNEL_SCAN] = clamp_servo_us(us);
    
    uint16_t flags = FLAG_CLAMP_ENABLE | FLAG_SCAN_ENABLE;
    if (g_estop.load()) flags |= FLAG_ESTOP;
    PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
    sendPosePacket(pkt);
}

void BrainDaemon::onSerialEstop() {
    g_estop.store(true);
    m_mailbox.sendEstop();
}

void BrainDaemon::onSerialResume() {
    g_estop.store(false);
}

std::string BrainDaemon::onSerialStatus() {
    char status[128];
    snprintf(status, sizeof(status),
        "seq=%u tx=%u ring_w=%u ring_r=%u clients=%zu estop=%s",
        m_seq, m_mailbox.getTxCount(),
        m_shared_mem.getWriteIdx(), m_shared_mem.getReadIdx(),
        m_clients.size(), g_estop.load() ? "active" : "clear");
    return std::string(status);
}

int BrainDaemon::onSerialDistance() {
    if (!m_distance_available) {
        return -1;
    }
    
    uint16_t distance_mm = 0;
    DistanceSensor::Status status = m_distance_sensor.readRange(distance_mm);
    
    if (status == DistanceSensor::Status::OK) {
        return (int)distance_mm;
    }
    return -1;
}

void BrainDaemon::initScanController() {
    // Configure scan profile
    ScanController::ScanProfile profile;
    profile.min_deg = 20;
    profile.max_deg = 160;
    profile.step_deg = 10;
    profile.rate_hz = 5;
    profile.dwell_ms = 80;
    m_scan_controller.setProfile(profile);
    
    // Set callback to move scan servo
    m_scan_controller.setServoCallback([this](int angle_deg) {
        setScanServoAngle(angle_deg);
    });
    
    // Set callback to read distance
    m_scan_controller.setDistanceCallback([this]() -> int {
        if (!m_distance_available) return -1;
        uint16_t distance_mm = 0;
        DistanceSensor::Status status = m_distance_sensor.readRange(distance_mm);
        if (status == DistanceSensor::Status::OK) {
            return (int)distance_mm;
        }
        return -1;
    });
    
    // Set callback for new scan data (optional: broadcast to clients)
    m_scan_controller.setDataCallback([this](const ScanController::ScanPoint& point) {
        // Optionally broadcast scan data to WebSocket clients
        char msg[128];
        snprintf(msg, sizeof(msg),
            "{\"type\":\"scan_data\",\"angle\":%d,\"distance\":%d}",
            point.angle_deg, point.distance_mm);
        wsBroadcast(msg);
    });
    
    LOG_INFO("Scan", "Controller initialized (not started)");
}

void BrainDaemon::setScanServoAngle(int angle_deg) {
    // Convert angle (0-180) to microseconds (500-2500)
    int us = 500 + (angle_deg * 2000 / 180);
    if (us < SERVO_PWM_MIN_US) us = SERVO_PWM_MIN_US;
    if (us > SERVO_PWM_MAX_US) us = SERVO_PWM_MAX_US;
    
    m_current_servos[SERVO_CHANNEL_SCAN] = (uint16_t)us;
    
    uint16_t flags = FLAG_CLAMP_ENABLE | FLAG_SCAN_ENABLE;
    if (g_estop.load()) flags |= FLAG_ESTOP;
    PosePacket31 pkt = buildPosePacket(0, flags, m_current_servos);
    sendPosePacket(pkt);
}

void BrainDaemon::handleScanCommand(const std::string& cmd) {
    // scan_start: {"type":"scan_start"} or {"type":"scan_start","profile":{...}}
    if (hasType(cmd, "scan_start")) {
        // Optionally update profile from command
        int min_deg = parseJsonInt(cmd, "min_deg", -1);
        int max_deg = parseJsonInt(cmd, "max_deg", -1);
        int step_deg = parseJsonInt(cmd, "step_deg", -1);
        int rate_hz = parseJsonInt(cmd, "rate_hz", -1);
        
        if (min_deg >= 0 || max_deg >= 0 || step_deg >= 0 || rate_hz >= 0) {
            ScanController::ScanProfile profile = m_scan_controller.getProfile();
            if (min_deg >= 0) profile.min_deg = min_deg;
            if (max_deg >= 0) profile.max_deg = max_deg;
            if (step_deg >= 0) profile.step_deg = step_deg;
            if (rate_hz >= 0) profile.rate_hz = rate_hz;
            m_scan_controller.setProfile(profile);
        }
        
        m_scan_controller.start();
        wsBroadcast("{\"status\":\"ok\",\"scan\":\"started\"}");
        return;
    }
    
    // scan_stop: {"type":"scan_stop"}
    if (hasType(cmd, "scan_stop")) {
        m_scan_controller.stop();
        wsBroadcast("{\"status\":\"ok\",\"scan\":\"stopped\"}");
        return;
    }
    
    // scan_status: {"type":"scan_status"}
    if (hasType(cmd, "scan_status")) {
        const auto& data = m_scan_controller.getScanData();
        char msg[256];
        snprintf(msg, sizeof(msg),
            "{\"scan_running\":%s,\"angle\":%d,\"closest_dist\":%d,\"closest_angle\":%d,\"points\":%zu}",
            m_scan_controller.isRunning() ? "true" : "false",
            m_scan_controller.getCurrentAngle(),
            m_scan_controller.getClosestDistance(),
            m_scan_controller.getClosestAngle(),
            data.size());
        wsBroadcast(msg);
        return;
    }
    
    // scan_data: {"type":"scan_data"} - get full scan data
    if (hasType(cmd, "scan_get_data")) {
        const auto& data = m_scan_controller.getScanData();
        std::string msg = "{\"scan_data\":[";
        for (size_t i = 0; i < data.size(); i++) {
            if (i > 0) msg += ",";
            char pt[48];
            snprintf(pt, sizeof(pt), "{\"a\":%d,\"d\":%d}",
                data[i].angle_deg, data[i].distance_mm);
            msg += pt;
        }
        msg += "]}";
        wsBroadcast(msg);
        return;
    }
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --log-level LEVEL   Set log level: DEBUG, INFO, WARN, ERROR (default: INFO)\n"
              << "  --log-file PATH     Log to file (in addition to stdout)\n"
              << "  --serial-port PORT  Serial port for control (default: " << DEFAULT_SERIAL_PORT << ")\n"
              << "  --serial-baud BAUD  Serial baud rate (default: " << DEFAULT_SERIAL_BAUD << ")\n"
              << "  -h, --help          Show this help\n";
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"log-level",   required_argument, 0, 'l'},
        {"log-file",    required_argument, 0, 'f'},
        {"serial-port", required_argument, 0, 's'},
        {"serial-baud", required_argument, 0, 'b'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    std::string log_level = "INFO";
    std::string log_file;
    std::string serial_port = DEFAULT_SERIAL_PORT;
    int serial_baud = DEFAULT_SERIAL_BAUD;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "l:f:s:b:h", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'l':
            log_level = optarg;
            break;
        case 'f':
            log_file = optarg;
            break;
        case 's':
            serial_port = optarg;
            break;
        case 'b':
            serial_baud = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }
    
    Logger::instance().setLevel(log_level);
    if (!log_file.empty()) {
        if (!Logger::instance().openFile(log_file)) {
            std::cerr << "Warning: Could not open log file: " << log_file << std::endl;
        }
    }
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    BrainDaemon daemon;
    daemon.setSerialPort(serial_port);
    daemon.setSerialBaud(serial_baud);
    
    if (!daemon.init()) {
        LOG_ERROR("Brain", "Initialization failed");
        return 1;
    }
    
    daemon.run();
    daemon.shutdown();
    
    LOG_INFO("Brain", "Exited cleanly");
    return 0;
}
