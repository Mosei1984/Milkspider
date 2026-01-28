/**
 * I2C HAL Implementation for Milk-V Duo (FreeRTOS)
 *
 * Uses SDK's hal_dw_i2c driver
 */

#include <stdint.h>
#include "i2c_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* Include SDK I2C HAL - declare functions manually to avoid include issues */
extern void hal_i2c_init(uint8_t i2c_id);
extern int hal_i2c_write(uint8_t i2c_id, uint8_t dev, uint16_t addr, uint16_t alen, uint8_t *buffer, uint16_t len);
extern int hal_i2c_read(uint8_t i2c_id, uint8_t dev, uint16_t addr, uint16_t alen, uint8_t *buffer, uint16_t len);

// I2C1 is used for PCA9685 (GP4=SCL, GP5=SDA on Milk-V Duo 256M)
#define I2C_BUS_ID  1

static uint8_t s_initialized = 0;

int i2c_hal_init(void) {
    if (s_initialized) {
        return 0;
    }

    hal_i2c_init(I2C_BUS_ID);
    s_initialized = 1;
    return 0;
}

int i2c_hal_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t value) {
    return hal_i2c_write(I2C_BUS_ID, dev_addr, reg, 1, &value, 1);
}

int i2c_hal_write_buf(uint8_t dev_addr, uint8_t reg, const uint8_t *data, size_t len) {
    return hal_i2c_write(I2C_BUS_ID, dev_addr, reg, 1, (uint8_t *)data, (uint16_t)len);
}

int i2c_hal_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *value, size_t len) {
    return hal_i2c_read(I2C_BUS_ID, dev_addr, reg, 1, value, (uint16_t)len);
}

void i2c_hal_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
