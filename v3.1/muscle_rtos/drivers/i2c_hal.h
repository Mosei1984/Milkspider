#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * I2C Hardware Abstraction Layer for Milk-V Duo FreeRTOS
 *
 * Wraps platform-specific I2C driver (cv180x I2C peripheral).
 */

/**
 * Initialize I2C peripheral (I2C1 on Milk-V Duo).
 * Returns 0 on success.
 */
int i2c_hal_init(void);

/**
 * Write a single register value.
 */
int i2c_hal_write_reg(uint8_t addr, uint8_t reg, uint8_t value);

/**
 * Write buffer to starting register (with auto-increment).
 */
int i2c_hal_write_buf(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

/**
 * Read registers into buffer.
 */
int i2c_hal_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

/**
 * Delay in milliseconds (uses FreeRTOS vTaskDelay).
 */
void i2c_hal_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // I2C_HAL_H
