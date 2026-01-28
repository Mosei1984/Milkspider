/**
 * Spider Robot v3.1 - Serial Control Interface
 */

#include "serial_control.h"
#include "eye_client.h"
#include "logger.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <vector>

extern "C" {
#include "limits.h"
}

static const char* TAG = "Serial";

SerialControl::SerialControl()
    : m_port("/dev/ttyS0")
    , m_baud(115200)
    , m_fd(-1)
    , m_rx_len(0)
    , m_eye_client(nullptr)
{
}

SerialControl::~SerialControl() {
    shutdown();
}

bool SerialControl::init() {
    m_fd = open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        LOG_WARN(TAG, "Failed to open %s: %s (serial control disabled)", 
                 m_port.c_str(), strerror(errno));
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(m_fd, &tty) != 0) {
        LOG_ERROR(TAG, "tcgetattr failed: %s", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    speed_t baud_code;
    switch (m_baud) {
        case 9600:   baud_code = B9600;   break;
        case 19200:  baud_code = B19200;  break;
        case 38400:  baud_code = B38400;  break;
        case 57600:  baud_code = B57600;  break;
        case 115200: baud_code = B115200; break;
        case 230400: baud_code = B230400; break;
        case 460800: baud_code = B460800; break;
        default:
            LOG_WARN(TAG, "Unknown baud rate %d, using 115200", m_baud);
            baud_code = B115200;
    }

    cfsetospeed(&tty, baud_code);
    cfsetispeed(&tty, baud_code);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        LOG_ERROR(TAG, "tcsetattr failed: %s", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    tcflush(m_fd, TCIOFLUSH);

    LOG_INFO(TAG, "Opened %s at %d baud", m_port.c_str(), m_baud);
    sendResponse("OK Spider v3.1 Serial Ready");
    return true;
}

void SerialControl::tick() {
    if (m_fd < 0) return;

    ssize_t n = read(m_fd, m_rx_buffer + m_rx_len, sizeof(m_rx_buffer) - m_rx_len - 1);
    if (n > 0) {
        m_rx_len += n;
        m_rx_buffer[m_rx_len] = '\0';

        char* line_start = m_rx_buffer;
        char* newline;
        
        while ((newline = strchr(line_start, '\n')) != nullptr) {
            *newline = '\0';
            size_t len = newline - line_start;
            if (len > 0 && line_start[len - 1] == '\r') {
                line_start[len - 1] = '\0';
            }
            
            if (strlen(line_start) > 0) {
                processLine(line_start);
            }
            
            line_start = newline + 1;
        }

        size_t remaining = m_rx_buffer + m_rx_len - line_start;
        if (remaining > 0 && line_start != m_rx_buffer) {
            memmove(m_rx_buffer, line_start, remaining);
        }
        m_rx_len = remaining;

        if (m_rx_len >= sizeof(m_rx_buffer) - 1) {
            LOG_WARN(TAG, "RX buffer overflow, clearing");
            m_rx_len = 0;
        }
    }
}

void SerialControl::shutdown() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
        LOG_INFO(TAG, "Closed serial port");
    }
}

void SerialControl::sendResponse(const std::string& response) {
    if (m_fd < 0) return;
    
    std::string msg = response + "\r\n";
    write(m_fd, msg.c_str(), msg.length());
}

void SerialControl::processLine(const std::string& line) {
    LOG_DEBUG(TAG, "Received: %s", line.c_str());

    std::string cmd;
    std::string args;
    
    size_t space_pos = line.find(' ');
    if (space_pos != std::string::npos) {
        cmd = line.substr(0, space_pos);
        args = line.substr(space_pos + 1);
    } else {
        cmd = line;
    }

    for (char& c : cmd) {
        c = toupper(c);
    }

    bool ok = false;
    
    if (cmd == "STATUS") {
        ok = handleStatus();
    } else if (cmd == "SERVO") {
        ok = handleServo(args);
    } else if (cmd == "SERVOS") {
        ok = handleServos(args);
    } else if (cmd == "MOVE") {
        ok = handleMove(args);
    } else if (cmd == "SCAN") {
        ok = handleScan(args);
    } else if (cmd == "ESTOP") {
        ok = handleEstop();
    } else if (cmd == "RESUME") {
        ok = handleResume();
    } else if (cmd == "EYE") {
        ok = handleEye(args);
    } else if (cmd == "DISTANCE") {
        ok = handleDistance();
    } else if (cmd == "HELP" || cmd == "?") {
        sendResponse("OK Commands: STATUS SERVO SERVOS MOVE SCAN ESTOP RESUME EYE DISTANCE");
        ok = true;
    } else {
        sendResponse("ERR unknown command");
        return;
    }
    
    (void)ok;
}

bool SerialControl::handleStatus() {
    if (m_status_cb) {
        std::string status = m_status_cb();
        sendResponse("OK " + status);
        return true;
    }
    sendResponse("OK ready");
    return true;
}

bool SerialControl::handleServo(const std::string& args) {
    int channel = -1;
    int us = -1;
    
    if (sscanf(args.c_str(), "%d %d", &channel, &us) != 2) {
        sendResponse("ERR usage: SERVO <ch> <us>");
        return false;
    }
    
    if (channel < 0 || channel >= SERVO_COUNT_TOTAL) {
        sendResponse("ERR invalid channel (0-12)");
        return false;
    }
    
    if (us < SERVO_PWM_MIN_US || us > SERVO_PWM_MAX_US) {
        sendResponse("ERR us out of range (500-2500)");
        return false;
    }
    
    if (m_servo_cb) {
        m_servo_cb(channel, (uint16_t)us);
    }
    
    char resp[32];
    snprintf(resp, sizeof(resp), "OK %d %d", channel, us);
    sendResponse(resp);
    return true;
}

bool SerialControl::handleServos(const std::string& args) {
    std::vector<uint16_t> values;
    std::istringstream iss(args);
    int val;
    
    while (iss >> val) {
        if (val < SERVO_PWM_MIN_US || val > SERVO_PWM_MAX_US) {
            sendResponse("ERR value out of range (500-2500)");
            return false;
        }
        values.push_back((uint16_t)val);
    }
    
    if ((int)values.size() != SERVO_COUNT_TOTAL) {
        char err[64];
        snprintf(err, sizeof(err), "ERR expected %d values, got %zu", 
                 SERVO_COUNT_TOTAL, values.size());
        sendResponse(err);
        return false;
    }
    
    if (m_servos_cb) {
        m_servos_cb(values.data(), SERVO_COUNT_TOTAL);
    }
    
    sendResponse("OK");
    return true;
}

bool SerialControl::handleMove(const std::string& args) {
    std::vector<int> values;
    std::istringstream iss(args);
    int val;
    
    while (iss >> val) {
        values.push_back(val);
    }
    
    if ((int)values.size() != SERVO_COUNT_TOTAL + 1) {
        char err[64];
        snprintf(err, sizeof(err), "ERR expected t_ms + %d values, got %zu", 
                 SERVO_COUNT_TOTAL, values.size());
        sendResponse(err);
        return false;
    }
    
    uint32_t t_ms = (uint32_t)values[0];
    std::vector<uint16_t> servos;
    
    for (int i = 1; i <= SERVO_COUNT_TOTAL; i++) {
        int us = values[i];
        if (us < SERVO_PWM_MIN_US || us > SERVO_PWM_MAX_US) {
            sendResponse("ERR servo value out of range (500-2500)");
            return false;
        }
        servos.push_back((uint16_t)us);
    }
    
    if (m_move_cb) {
        m_move_cb(t_ms, servos.data(), SERVO_COUNT_TOTAL);
    }
    
    char resp[32];
    snprintf(resp, sizeof(resp), "OK t=%u", t_ms);
    sendResponse(resp);
    return true;
}

bool SerialControl::handleScan(const std::string& args) {
    int us = -1;
    
    if (sscanf(args.c_str(), "%d", &us) != 1) {
        sendResponse("ERR usage: SCAN <us>");
        return false;
    }
    
    if (us < SERVO_PWM_MIN_US || us > SERVO_PWM_MAX_US) {
        sendResponse("ERR us out of range (500-2500)");
        return false;
    }
    
    if (m_scan_cb) {
        m_scan_cb((uint16_t)us);
    }
    
    char resp[32];
    snprintf(resp, sizeof(resp), "OK scan=%d", us);
    sendResponse(resp);
    return true;
}

bool SerialControl::handleEstop() {
    if (m_estop_cb) {
        m_estop_cb();
    }
    sendResponse("OK ESTOP");
    return true;
}

bool SerialControl::handleResume() {
    if (m_resume_cb) {
        m_resume_cb();
    }
    sendResponse("OK RESUMED");
    return true;
}

bool SerialControl::handleEye(const std::string& args) {
    if (!m_eye_client) {
        sendResponse("ERR eye service unavailable");
        return false;
    }
    
    std::string subcmd;
    std::string subargs;
    
    size_t space_pos = args.find(' ');
    if (space_pos != std::string::npos) {
        subcmd = args.substr(0, space_pos);
        subargs = args.substr(space_pos + 1);
    } else {
        subcmd = args;
    }
    
    for (char& c : subcmd) {
        c = toupper(c);
    }
    
    if (subcmd == "MOOD") {
        std::string mood = subargs;
        for (char& c : mood) {
            c = tolower(c);
        }
        
        if (mood != "normal" && mood != "angry" && mood != "happy" && mood != "sleepy") {
            sendResponse("ERR mood must be: normal|angry|happy|sleepy");
            return false;
        }
        
        if (m_eye_client->setMood(mood)) {
            sendResponse("OK mood=" + mood);
            return true;
        } else {
            sendResponse("ERR eye command failed");
            return false;
        }
    }
    else if (subcmd == "LOOK") {
        float x = 0.0f, y = 0.0f;
        if (sscanf(subargs.c_str(), "%f %f", &x, &y) != 2) {
            sendResponse("ERR usage: EYE LOOK <x> <y>");
            return false;
        }
        
        if (x < -1.0f) x = -1.0f;
        if (x >  1.0f) x =  1.0f;
        if (y < -1.0f) y = -1.0f;
        if (y >  1.0f) y =  1.0f;
        
        if (m_eye_client->lookAt(x, y)) {
            char resp[64];
            snprintf(resp, sizeof(resp), "OK look=%.2f,%.2f", x, y);
            sendResponse(resp);
            return true;
        } else {
            sendResponse("ERR eye command failed");
            return false;
        }
    }
    else if (subcmd == "BLINK") {
        if (m_eye_client->blink()) {
            sendResponse("OK blink");
            return true;
        } else {
            sendResponse("ERR eye command failed");
            return false;
        }
    }
    else if (subcmd == "WINK") {
        std::string eye = subargs;
        for (char& c : eye) {
            c = tolower(c);
        }
        
        if (eye != "left" && eye != "right") {
            sendResponse("ERR wink must be: left|right");
            return false;
        }
        
        if (m_eye_client->wink(eye)) {
            sendResponse("OK wink=" + eye);
            return true;
        } else {
            sendResponse("ERR eye command failed");
            return false;
        }
    }
    else {
        sendResponse("ERR unknown eye command (MOOD|LOOK|BLINK|WINK)");
        return false;
    }
}

bool SerialControl::handleDistance() {
    if (m_distance_cb) {
        int distance = m_distance_cb();
        if (distance >= 0) {
            char resp[32];
            snprintf(resp, sizeof(resp), "OK %d", distance);
            sendResponse(resp);
            return true;
        } else {
            sendResponse("ERR distance read failed");
            return false;
        }
    }
    sendResponse("ERR distance sensor unavailable");
    return false;
}
