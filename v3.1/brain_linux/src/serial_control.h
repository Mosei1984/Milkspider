/**
 * Spider Robot v3.1 - Serial Control Interface
 * 
 * Text-based serial control for embedded controllers.
 * Runs alongside WebSocket server, supports same command set.
 */

#ifndef SERIAL_CONTROL_H
#define SERIAL_CONTROL_H

#include <string>
#include <functional>
#include <cstdint>

class EyeClient;

class SerialControl {
public:
    using ServoCallback = std::function<void(int channel, uint16_t us)>;
    using ServosCallback = std::function<void(const uint16_t* us, int count)>;
    using MoveCallback = std::function<void(uint32_t t_ms, const uint16_t* us, int count)>;
    using ScanCallback = std::function<void(uint16_t us)>;
    using EstopCallback = std::function<void()>;
    using ResumeCallback = std::function<void()>;
    using StatusCallback = std::function<std::string()>;
    using DistanceCallback = std::function<int()>;

    SerialControl();
    ~SerialControl();

    void setPort(const std::string& port) { m_port = port; }
    void setBaudRate(int baud) { m_baud = baud; }
    void setEyeClient(EyeClient* eye) { m_eye_client = eye; }

    void setServoCallback(ServoCallback cb) { m_servo_cb = cb; }
    void setServosCallback(ServosCallback cb) { m_servos_cb = cb; }
    void setMoveCallback(MoveCallback cb) { m_move_cb = cb; }
    void setScanCallback(ScanCallback cb) { m_scan_cb = cb; }
    void setEstopCallback(EstopCallback cb) { m_estop_cb = cb; }
    void setResumeCallback(ResumeCallback cb) { m_resume_cb = cb; }
    void setStatusCallback(StatusCallback cb) { m_status_cb = cb; }
    void setDistanceCallback(DistanceCallback cb) { m_distance_cb = cb; }

    bool init();
    void tick();
    void shutdown();

private:
    void processLine(const std::string& line);
    void sendResponse(const std::string& response);
    
    bool handleStatus();
    bool handleServo(const std::string& args);
    bool handleServos(const std::string& args);
    bool handleMove(const std::string& args);
    bool handleScan(const std::string& args);
    bool handleEstop();
    bool handleResume();
    bool handleEye(const std::string& args);
    bool handleDistance();

    std::string m_port;
    int m_baud;
    int m_fd;
    
    char m_rx_buffer[1024];
    size_t m_rx_len;
    
    EyeClient* m_eye_client;
    
    ServoCallback m_servo_cb;
    ServosCallback m_servos_cb;
    MoveCallback m_move_cb;
    ScanCallback m_scan_cb;
    EstopCallback m_estop_cb;
    ResumeCallback m_resume_cb;
    StatusCallback m_status_cb;
    DistanceCallback m_distance_cb;
};

#endif // SERIAL_CONTROL_H
