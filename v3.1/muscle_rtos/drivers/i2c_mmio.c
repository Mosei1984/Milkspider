/**
 * I2C MMIO Driver for CV181x (Milk-V Duo 256M)
 * 
 * Direct register access implementation - no SDK HAL dependency.
 * Uses DesignWare I2C controller at I2C1 (0x04010000).
 * 
 * Pins: GP4 = I2C1_SCL, GP5 = I2C1_SDA
 */

#include <stdint.h>
#include "i2c_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define I2C1_BASE       0x04010000UL

#define IC_CON          0x00
#define IC_TAR          0x04
#define IC_DATA_CMD     0x10
#define IC_SS_SCL_HCNT  0x14
#define IC_SS_SCL_LCNT  0x18
#define IC_FS_SCL_HCNT  0x1C
#define IC_FS_SCL_LCNT  0x20
#define IC_INTR_STAT    0x2C
#define IC_INTR_MASK    0x30
#define IC_RAW_INTR_STAT 0x34
#define IC_RX_TL        0x38
#define IC_TX_TL        0x3C
#define IC_CLR_INTR     0x40
#define IC_CLR_TX_ABRT  0x54
#define IC_ENABLE       0x6C
#define IC_STATUS       0x70
#define IC_TXFLR        0x74
#define IC_RXFLR        0x78
#define IC_TX_ABRT_SRC  0x80
#define IC_ENABLE_STATUS 0x9C
#define IC_COMP_PARAM_1 0xF4

#define IC_CON_MASTER_MODE      (1 << 0)
#define IC_CON_SPEED_SS         (1 << 1)
#define IC_CON_SPEED_FS         (2 << 1)
#define IC_CON_IC_RESTART_EN    (1 << 5)
#define IC_CON_IC_SLAVE_DISABLE (1 << 6)

#define IC_DATA_CMD_READ        (1 << 8)
#define IC_DATA_CMD_STOP        (1 << 9)

#define IC_STATUS_TFE           (1 << 2)
#define IC_STATUS_TFNF          (1 << 1)
#define IC_STATUS_RFNE          (1 << 3)
#define IC_STATUS_MST_ACTIVITY  (1 << 5)

#define IC_RAW_TX_ABRT          (1 << 6)

#define PINMUX_BASE         0x03001000UL
#define GP4_PINMUX_OFFSET   0x0E0
#define GP5_PINMUX_OFFSET   0x0E4
#define FMUX_I2C1_SCL       3
#define FMUX_I2C1_SDA       3

#define CLK_DIV_BASE        0x03002000UL
#define CLK_I2C1_DIV_OFFSET 0x80

#define I2C_TIMEOUT_MS      100

static volatile uint32_t *i2c_base = (volatile uint32_t *)I2C1_BASE;
static uint8_t s_initialized = 0;

static inline void mmio_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(I2C1_BASE + offset) = value;
    __asm__ __volatile__("fence w,o" ::: "memory");
}

static inline uint32_t mmio_read(uint32_t offset) {
    __asm__ __volatile__("fence i,r" ::: "memory");
    return *(volatile uint32_t *)(I2C1_BASE + offset);
}

static inline void pinmux_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(PINMUX_BASE + offset) = value;
    __asm__ __volatile__("fence w,o" ::: "memory");
}

static void i2c_disable(void) {
    int timeout = 100;
    mmio_write(IC_ENABLE, 0);
    while ((mmio_read(IC_ENABLE_STATUS) & 1) && --timeout > 0) {
        vTaskDelay(1);
    }
}

static void i2c_enable(void) {
    mmio_write(IC_ENABLE, 1);
}

static int i2c_wait_idle(void) {
    uint32_t start = xTaskGetTickCount();
    while (mmio_read(IC_STATUS) & IC_STATUS_MST_ACTIVITY) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(I2C_TIMEOUT_MS)) {
            return -1;
        }
        vTaskDelay(1);
    }
    return 0;
}

static int i2c_wait_tx_empty(void) {
    uint32_t start = xTaskGetTickCount();
    while (!(mmio_read(IC_STATUS) & IC_STATUS_TFE)) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(I2C_TIMEOUT_MS)) {
            return -1;
        }
        if (mmio_read(IC_RAW_INTR_STAT) & IC_RAW_TX_ABRT) {
            mmio_read(IC_CLR_TX_ABRT);
            return -2;
        }
        vTaskDelay(1);
    }
    return 0;
}

int i2c_hal_init(void) {
    if (s_initialized) {
        return 0;
    }

    pinmux_write(GP4_PINMUX_OFFSET, FMUX_I2C1_SCL);
    pinmux_write(GP5_PINMUX_OFFSET, FMUX_I2C1_SDA);

    i2c_disable();

    uint32_t con = IC_CON_MASTER_MODE | IC_CON_SPEED_FS |
                   IC_CON_IC_RESTART_EN | IC_CON_IC_SLAVE_DISABLE;
    mmio_write(IC_CON, con);

    mmio_write(IC_FS_SCL_HCNT, 60);
    mmio_write(IC_FS_SCL_LCNT, 130);

    mmio_write(IC_RX_TL, 0);
    mmio_write(IC_TX_TL, 0);

    mmio_write(IC_INTR_MASK, 0);

    i2c_enable();

    s_initialized = 1;
    return 0;
}

int i2c_hal_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t value) {
    if (i2c_wait_idle() != 0) return -1;

    i2c_disable();
    mmio_write(IC_TAR, dev_addr);
    i2c_enable();

    mmio_write(IC_DATA_CMD, reg);
    mmio_write(IC_DATA_CMD, value | IC_DATA_CMD_STOP);

    if (i2c_wait_tx_empty() != 0) return -2;
    if (i2c_wait_idle() != 0) return -3;

    return 0;
}

int i2c_hal_write_buf(uint8_t dev_addr, uint8_t reg, const uint8_t *data, size_t len) {
    if (len == 0) return 0;
    if (i2c_wait_idle() != 0) return -1;

    i2c_disable();
    mmio_write(IC_TAR, dev_addr);
    i2c_enable();

    mmio_write(IC_DATA_CMD, reg);

    for (size_t i = 0; i < len; i++) {
        uint32_t cmd = data[i];
        if (i == len - 1) {
            cmd |= IC_DATA_CMD_STOP;
        }
        while (!(mmio_read(IC_STATUS) & IC_STATUS_TFNF)) {
            vTaskDelay(1);
        }
        mmio_write(IC_DATA_CMD, cmd);
    }

    if (i2c_wait_tx_empty() != 0) return -2;
    if (i2c_wait_idle() != 0) return -3;

    return 0;
}

int i2c_hal_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len) {
    if (len == 0) return 0;
    if (i2c_wait_idle() != 0) return -1;

    i2c_disable();
    mmio_write(IC_TAR, dev_addr);
    i2c_enable();

    mmio_write(IC_DATA_CMD, reg);

    for (size_t i = 0; i < len; i++) {
        uint32_t cmd = IC_DATA_CMD_READ;
        if (i == len - 1) {
            cmd |= IC_DATA_CMD_STOP;
        }
        mmio_write(IC_DATA_CMD, cmd);
    }

    for (size_t i = 0; i < len; i++) {
        uint32_t start = xTaskGetTickCount();
        while (!(mmio_read(IC_STATUS) & IC_STATUS_RFNE)) {
            if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(I2C_TIMEOUT_MS)) {
                return -2;
            }
            vTaskDelay(1);
        }
        data[i] = (uint8_t)mmio_read(IC_DATA_CMD);
    }

    if (i2c_wait_idle() != 0) return -3;

    return 0;
}

void i2c_hal_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
