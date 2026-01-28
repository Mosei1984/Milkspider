#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9685_I2C_ADDR_DEFAULT  0x40
#define PCA9685_CHANNEL_COUNT     16
#define PCA9685_FREQ_HZ           50   // Standard servo frequency

/**
 * Initialize PCA9685 on I2C bus.
 * Returns 0 on success, negative on error.
 */
int pca9685_init(uint8_t i2c_addr);

/**
 * Set PWM for a channel in microseconds (500-2500 µs).
 * Automatically converts to PCA9685 tick counts.
 */
int pca9685_set_pwm_us(uint8_t channel, uint16_t pulse_us);

/**
 * Set PWM for a channel using raw on/off tick values (0-4095).
 */
int pca9685_set_pwm_raw(uint8_t channel, uint16_t on, uint16_t off);

/**
 * Set all channels to the same pulse width (for neutral/safe pose).
 */
void pca9685_set_all_us(uint16_t pulse_us);

/**
 * Put PCA9685 into sleep mode (low power).
 */
void pca9685_sleep(void);

/**
 * Wake PCA9685 from sleep mode.
 */
void pca9685_wake(void);

#ifdef __cplusplus
}
#endif

#endif // PCA9685_H
