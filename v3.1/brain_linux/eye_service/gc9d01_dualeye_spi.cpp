/**
 * GC9D01DualEyeSpi Implementation
 *
 * Uses Linux spidev and GPIO sysfs/gpiod for control.
 */

#include "gc9d01_dualeye_spi.hpp"
#include "gpio/gpio_backend.hpp"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>

// SPI settings
#define SPI_DEVICE    "/dev/spidev0.0"
#define SPI_SPEED_HZ  20000000  // 20 MHz
#define SPI_MODE      SPI_MODE_0
#define SPI_BITS      8

// GPIO pin assignments (from PINOUT.md)
#define GPIO_DC        506  // J2-27 XGPIOA[26]
#define GPIO_CS_LEFT   505  // J2-25 XGPIOA[25]
#define GPIO_CS_RIGHT  507  // J2-26 XGPIOA[27]
#define GPIO_RST_LEFT  451  // J2-31 XGPIOB[3]
#define GPIO_RST_RIGHT 454  // J2-32 XGPIOB[6]

// GC9D01 commands
#define CMD_SLPOUT    0x11
#define CMD_DISPON    0x29
#define CMD_CASET     0x2A
#define CMD_RASET     0x2B
#define CMD_RAMWR     0x2C
#define CMD_MADCTL    0x36
#define CMD_COLMOD    0x3A

static GpioBackend *s_gpio = nullptr;

GC9D01DualEyeSpi::GC9D01DualEyeSpi() {
    s_gpio = createGpioBackend();
}

GC9D01DualEyeSpi::~GC9D01DualEyeSpi() {
    if (m_spi_fd >= 0) {
        close(m_spi_fd);
    }
    if (s_gpio) {
        s_gpio->cleanup();
        delete s_gpio;
        s_gpio = nullptr;
    }
}

bool GC9D01DualEyeSpi::init() {
    // Initialize GPIO subsystem
    if (!s_gpio || !s_gpio->init()) {
        std::cerr << "[Display] Failed to initialize GPIO backend" << std::endl;
        return false;
    }

    // Configure all GPIO pins as outputs
    if (!s_gpio->configurePin(GPIO_DC, GpioBackend::Direction::OUTPUT)) {
        std::cerr << "[Display] Failed to configure DC pin" << std::endl;
        return false;
    }
    if (!s_gpio->configurePin(GPIO_CS_LEFT, GpioBackend::Direction::OUTPUT)) {
        std::cerr << "[Display] Failed to configure CS_LEFT pin" << std::endl;
        return false;
    }
    if (!s_gpio->configurePin(GPIO_CS_RIGHT, GpioBackend::Direction::OUTPUT)) {
        std::cerr << "[Display] Failed to configure CS_RIGHT pin" << std::endl;
        return false;
    }
    if (!s_gpio->configurePin(GPIO_RST_LEFT, GpioBackend::Direction::OUTPUT)) {
        std::cerr << "[Display] Failed to configure RST_LEFT pin" << std::endl;
        return false;
    }
    if (!s_gpio->configurePin(GPIO_RST_RIGHT, GpioBackend::Direction::OUTPUT)) {
        std::cerr << "[Display] Failed to configure RST_RIGHT pin" << std::endl;
        return false;
    }

    // Set initial states: CS high (deselected), RST high (not reset)
    s_gpio->write(GPIO_CS_LEFT, 1);
    s_gpio->write(GPIO_CS_RIGHT, 1);
    s_gpio->write(GPIO_RST_LEFT, 1);
    s_gpio->write(GPIO_RST_RIGHT, 1);

    // Open SPI device
    m_spi_fd = open(SPI_DEVICE, O_RDWR);
    if (m_spi_fd < 0) {
        std::cerr << "[Display] Failed to open " << SPI_DEVICE << std::endl;
        return false;
    }

    // Configure SPI
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    ioctl(m_spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(m_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(m_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // Reset and initialize both displays
    if (!initDisplay(Eye::LEFT)) {
        return false;
    }
    if (!initDisplay(Eye::RIGHT)) {
        return false;
    }

    std::cout << "[Display] Initialized dual GC9D01 displays" << std::endl;
    return true;
}

bool GC9D01DualEyeSpi::initDisplay(Eye eye) {
    // Reset display
    reset(eye);

    selectEye(eye);

    // Sleep out
    sendCommand(CMD_SLPOUT);
    usleep(120000);  // 120ms

    // Pixel format: RGB565
    sendCommand(CMD_COLMOD);
    uint8_t colmod = 0x55;  // 16-bit color
    sendData(&colmod, 1);

    // Display on
    sendCommand(CMD_DISPON);
    usleep(20000);

    return true;
}

void GC9D01DualEyeSpi::selectEye(Eye eye) {
    // Deselect both (CS high = inactive)
    s_gpio->write(GPIO_CS_LEFT, 1);
    s_gpio->write(GPIO_CS_RIGHT, 1);

    // Select target (CS low = active)
    if (eye == Eye::LEFT) {
        s_gpio->write(GPIO_CS_LEFT, 0);
    } else {
        s_gpio->write(GPIO_CS_RIGHT, 0);
    }
}

void GC9D01DualEyeSpi::sendCommand(uint8_t cmd) {
    // DC low for command
    s_gpio->write(GPIO_DC, 0);

    struct spi_ioc_transfer xfer = {};
    xfer.tx_buf = reinterpret_cast<unsigned long>(&cmd);
    xfer.len = 1;
    xfer.speed_hz = SPI_SPEED_HZ;
    xfer.bits_per_word = SPI_BITS;

    ioctl(m_spi_fd, SPI_IOC_MESSAGE(1), &xfer);
}

void GC9D01DualEyeSpi::sendData(const uint8_t *data, size_t len) {
    // DC high for data
    s_gpio->write(GPIO_DC, 1);

    struct spi_ioc_transfer xfer = {};
    xfer.tx_buf = reinterpret_cast<unsigned long>(data);
    xfer.len = static_cast<uint32_t>(len);
    xfer.speed_hz = SPI_SPEED_HZ;
    xfer.bits_per_word = SPI_BITS;

    ioctl(m_spi_fd, SPI_IOC_MESSAGE(1), &xfer);
}

void GC9D01DualEyeSpi::reset(Eye eye) {
    int rst_pin = (eye == Eye::LEFT) ? GPIO_RST_LEFT : GPIO_RST_RIGHT;

    // Pull reset low
    s_gpio->write(rst_pin, 0);
    usleep(10000);  // 10ms low pulse

    // Release reset (high)
    s_gpio->write(rst_pin, 1);
    usleep(120000);  // 120ms recovery
}

bool GC9D01DualEyeSpi::writeFramebuffer(Eye eye, const uint16_t *buffer) {
    selectEye(eye);

    // Set column address (0 to WIDTH-1)
    sendCommand(CMD_CASET);
    uint8_t caset[] = {0, 0, 0, WIDTH - 1};
    sendData(caset, 4);

    // Set row address (0 to HEIGHT-1)
    sendCommand(CMD_RASET);
    uint8_t raset[] = {0, 0, 0, HEIGHT - 1};
    sendData(raset, 4);

    // Write memory
    sendCommand(CMD_RAMWR);

    // Swap bytes for RGB565 big-endian (display expects MSB first)
    uint8_t *swapped = new uint8_t[BUFFER_SIZE];
    for (int i = 0; i < PIXELS; i++) {
        swapped[i * 2] = (buffer[i] >> 8) & 0xFF;      // High byte first
        swapped[i * 2 + 1] = buffer[i] & 0xFF;         // Low byte second
    }
    sendData(swapped, BUFFER_SIZE);
    delete[] swapped;

    return true;
}

void GC9D01DualEyeSpi::setBacklight(Eye eye, uint8_t brightness) {
    // TODO: Implement PWM backlight control
    (void)eye;
    (void)brightness;
}

void GC9D01DualEyeSpi::fill(Eye eye, uint16_t color) {
    uint16_t buffer[PIXELS];
    for (int i = 0; i < PIXELS; i++) {
        buffer[i] = color;
    }
    writeFramebuffer(eye, buffer);
}
