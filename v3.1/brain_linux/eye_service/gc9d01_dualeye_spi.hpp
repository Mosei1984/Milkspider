#ifndef GC9D01_DUALEYE_SPI_HPP
#define GC9D01_DUALEYE_SPI_HPP

#include <cstdint>
#include <string>

/**
 * GC9D01DualEyeSpi - Dual GC9D01 display driver via SPI
 *
 * Two 160x160 RGB565 displays for left and right eyes.
 * Uses spidev for SPI communication and GPIO for control signals.
 */
class GC9D01DualEyeSpi {
public:
    static constexpr int WIDTH = 160;
    static constexpr int HEIGHT = 160;
    static constexpr int PIXELS = WIDTH * HEIGHT;
    static constexpr int BUFFER_SIZE = PIXELS * 2;  // RGB565 = 2 bytes/pixel

    enum class Eye {
        LEFT,
        RIGHT
    };

    GC9D01DualEyeSpi();
    ~GC9D01DualEyeSpi();

    /**
     * Initialize both displays.
     */
    bool init();

    /**
     * Write framebuffer to specified eye.
     */
    bool writeFramebuffer(Eye eye, const uint16_t *buffer);

    /**
     * Set backlight brightness (0-255).
     */
    void setBacklight(Eye eye, uint8_t brightness);

    /**
     * Fill display with solid color.
     */
    void fill(Eye eye, uint16_t color);

private:
    bool initDisplay(Eye eye);
    void selectEye(Eye eye);
    void sendCommand(uint8_t cmd);
    void sendData(const uint8_t *data, size_t len);
    void reset(Eye eye);

    int m_spi_fd = -1;
    int m_gpio_dc = -1;
    int m_gpio_cs_left = -1;
    int m_gpio_cs_right = -1;
    int m_gpio_rst_left = -1;
    int m_gpio_rst_right = -1;
};

#endif // GC9D01_DUALEYE_SPI_HPP
