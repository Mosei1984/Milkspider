#include "gpio_backend.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <set>

class GpioSysfs : public GpioBackend {
public:
    GpioSysfs() = default;
    ~GpioSysfs() override { cleanup(); }

    bool init() override {
        initialized_ = true;
        return true;
    }

    bool configurePin(int pin, Direction dir, Pull /*pull*/) override {
        if (!initialized_) return false;

        if (!exportPin(pin)) {
            return false;
        }
        exportedPins_.insert(pin);

        if (!setDirection(pin, dir)) {
            return false;
        }

        return true;
    }

    bool write(int pin, int value) override {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);

        int fd = open(path, O_WRONLY);
        if (fd < 0) {
            return false;
        }

        const char *val = (value != 0) ? "1" : "0";
        ssize_t written = ::write(fd, val, 1);
        close(fd);

        return written == 1;
    }

    int read(int pin) override {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            return -1;
        }

        char buf[4] = {0};
        ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n <= 0) {
            return -1;
        }

        return (buf[0] == '1') ? 1 : 0;
    }

    void cleanup() override {
        for (int pin : exportedPins_) {
            unexportPin(pin);
        }
        exportedPins_.clear();
        initialized_ = false;
    }

private:
    bool initialized_ = false;
    std::set<int> exportedPins_;

    bool exportPin(int pin) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", pin);
        if (access(path, F_OK) == 0) {
            return true;
        }

        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) {
            return false;
        }

        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d", pin);
        ssize_t written = ::write(fd, buf, len);
        close(fd);

        if (written != len) {
            return false;
        }

        usleep(50000);
        return true;
    }

    bool unexportPin(int pin) {
        int fd = open("/sys/class/gpio/unexport", O_WRONLY);
        if (fd < 0) {
            return false;
        }

        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d", pin);
        ssize_t written = ::write(fd, buf, len);
        close(fd);

        return written == len;
    }

    bool setDirection(int pin, Direction dir) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

        int fd = open(path, O_WRONLY);
        if (fd < 0) {
            return false;
        }

        const char *dirStr = (dir == Direction::OUTPUT) ? "out" : "in";
        ssize_t written = ::write(fd, dirStr, strlen(dirStr));
        close(fd);

        return written == static_cast<ssize_t>(strlen(dirStr));
    }
};

GpioBackend *createGpioBackend() {
    return new GpioSysfs();
}
