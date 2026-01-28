/**
 * PCA9685 16-Channel PWM Servo Driver
 *
 * I2C driver for FreeRTOS on Milk-V Duo.
 */

#include "pca9685.h"
#include "i2c_hal.h"
#include "limits.h"

// PCA9685 Registers
#define PCA9685_REG_MODE1       0x00
#define PCA9685_REG_MODE2       0x01
#define PCA9685_REG_LED0_ON_L   0x06
#define PCA9685_REG_PRESCALE    0xFE
#define PCA9685_REG_ALL_ON_L    0xFA

// MODE1 bits
#define MODE1_RESTART  0x80
#define MODE1_SLEEP    0x10
#define MODE1_AI       0x20  // Auto-increment

// MODE2 bits
#define MODE2_OUTDRV   0x04  // Totem pole outputs

// Internal oscillator: 25 MHz
#define PCA9685_OSC_FREQ  25000000
#define PCA9685_TICK_MAX  4096

static uint8_t s_i2c_addr = PCA9685_I2C_ADDR_DEFAULT;
static uint16_t s_ticks_per_us = 0;  // Pre-calculated for efficiency

int pca9685_init(uint8_t i2c_addr) {
    s_i2c_addr = i2c_addr;

    // Initialize I2C HAL
    if (i2c_hal_init() != 0) {
        return -1;
    }

    // Calculate prescale for 50 Hz
    // prescale = round(osc_freq / (4096 * update_rate)) - 1
    uint8_t prescale = (uint8_t)((PCA9685_OSC_FREQ / (PCA9685_TICK_MAX * PCA9685_FREQ_HZ)) - 1);

    // Put to sleep before changing prescale
    uint8_t mode1 = 0;
    i2c_hal_read_reg(s_i2c_addr, PCA9685_REG_MODE1, &mode1, 1);
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE1, (mode1 & ~MODE1_RESTART) | MODE1_SLEEP);

    // Set prescale
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_PRESCALE, prescale);

    // Wake up with auto-increment
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE1, MODE1_AI);

    // Wait for oscillator to stabilize
    i2c_hal_delay_ms(5);

    // Set MODE2 for totem-pole outputs
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE2, MODE2_OUTDRV);

    // Calculate ticks per microsecond
    // At 50 Hz, period = 20000 µs, 4096 ticks
    // ticks_per_us = 4096 / 20000 ≈ 0.2048
    // For integer math: use (pulse_us * 4096) / 20000
    s_ticks_per_us = 4096;  // Store for division later

    return 0;
}

int pca9685_set_pwm_us(uint8_t channel, uint16_t pulse_us) {
    if (channel >= PCA9685_CHANNEL_COUNT) {
        return -1;
    }

    // Clamp to safe range (MANDATORY - Muscle enforces this)
    if (pulse_us < SERVO_PWM_MIN_US) pulse_us = SERVO_PWM_MIN_US;
    if (pulse_us > SERVO_PWM_MAX_US) pulse_us = SERVO_PWM_MAX_US;

    // Convert µs to ticks: (pulse_us * 4096) / 20000
    uint16_t off_tick = (uint16_t)(((uint32_t)pulse_us * 4096) / 20000);

    return pca9685_set_pwm_raw(channel, 0, off_tick);
}

int pca9685_set_pwm_raw(uint8_t channel, uint16_t on, uint16_t off) {
    if (channel >= PCA9685_CHANNEL_COUNT) {
        return -1;
    }

    uint8_t reg = PCA9685_REG_LED0_ON_L + (channel * 4);
    uint8_t data[4] = {
        (uint8_t)(on & 0xFF),
        (uint8_t)(on >> 8),
        (uint8_t)(off & 0xFF),
        (uint8_t)(off >> 8)
    };

    return i2c_hal_write_buf(s_i2c_addr, reg, data, 4);
}

void pca9685_set_all_us(uint16_t pulse_us) {
    for (uint8_t i = 0; i < PCA9685_CHANNEL_COUNT; i++) {
        pca9685_set_pwm_us(i, pulse_us);
    }
}

void pca9685_sleep(void) {
    uint8_t mode1 = 0;
    i2c_hal_read_reg(s_i2c_addr, PCA9685_REG_MODE1, &mode1, 1);
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE1, mode1 | MODE1_SLEEP);
}

void pca9685_wake(void) {
    uint8_t mode1 = 0;
    i2c_hal_read_reg(s_i2c_addr, PCA9685_REG_MODE1, &mode1, 1);
    i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE1, mode1 & ~MODE1_SLEEP);
    i2c_hal_delay_ms(5);

    // Restart if needed
    if (mode1 & MODE1_RESTART) {
        i2c_hal_write_reg(s_i2c_addr, PCA9685_REG_MODE1, mode1 | MODE1_RESTART);
    }
}
