#ifndef GPIO_BACKEND_HPP
#define GPIO_BACKEND_HPP

#include <cstdint>

/**
 * GPIO Backend Interface
 *
 * Abstract interface for GPIO control.
 * Implementations: wiringx, gpiod, sysfs
 */
class GpioBackend {
public:
    virtual ~GpioBackend() = default;

    enum class Direction {
        INPUT,
        OUTPUT
    };

    enum class Pull {
        NONE,
        UP,
        DOWN
    };

    /**
     * Initialize GPIO subsystem.
     */
    virtual bool init() = 0;

    /**
     * Export and configure a GPIO pin.
     */
    virtual bool configurePin(int pin, Direction dir, Pull pull = Pull::NONE) = 0;

    /**
     * Write to output pin (0 or 1).
     */
    virtual bool write(int pin, int value) = 0;

    /**
     * Read from input pin.
     */
    virtual int read(int pin) = 0;

    /**
     * Cleanup GPIO subsystem.
     */
    virtual void cleanup() = 0;
};

/**
 * Create platform-specific GPIO backend.
 */
GpioBackend *createGpioBackend();

#endif // GPIO_BACKEND_HPP
