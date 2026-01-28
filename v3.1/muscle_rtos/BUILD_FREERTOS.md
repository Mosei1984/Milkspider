# Spider Robot v3.1 - FreeRTOS Build Guide

## ⚠️ CRITICAL: SDK Version Warning

**MUST use SDK v1 (duo-sdk-v1) only!**

- The Arduino image uses SDK v1 format
- SDK v2 produces incompatible `fip.bin` that **fails to boot**
- SDK v2 HAL I2C drivers have undefined symbols (`IC3_INTR`, `arch_usleep`)

---

## SDK Setup

```bash
# Clone or extract SDK v1 to ~/duo-sdk-v1/
git clone https://github.com/milkv-duo/duo-buildroot-sdk.git --depth=1 ~/duo-sdk-v1

# Set environment variable
export SDK_DIR=~/duo-sdk-v1
```

---

## File Locations

| Purpose | Path |
|---------|------|
| Main source | `$SDK_DIR/freertos/cvitek/task/comm/src/riscv64/app_main.c` |
| Drivers | `$SDK_DIR/freertos/cvitek/task/comm/src/riscv64/drivers/` |
| Safety | `$SDK_DIR/freertos/cvitek/task/comm/src/riscv64/safety/` |
| HAL | `$SDK_DIR/freertos/cvitek/hal/cv181x/` |
| Output | `$SDK_DIR/fsbl/build/cv1812cp_milkv_duo256m_sd/fip.bin` |

---

## SDK Patches Required

The SDK HAL I2C driver is broken. Apply these patches before building:

### 1. HAL CMakeLists.txt

Remove `i2c` from build_list (the HAL I2C driver has undefined symbols):

```bash
cp patches/hal_cmakelists.txt \
   ~/duo-sdk-v1/freertos/cvitek/hal/cv181x/CMakeLists.txt
```

### 2. Comm CMakeLists.txt

```bash
cp patches/comm_cmakelists.txt \
   ~/duo-sdk-v1/freertos/cvitek/task/comm/CMakeLists.txt
```

---

## Custom I2C Driver

The SDK's `hal_dw_i2c.c` has undefined symbols (`IC3_INTR`, `arch_usleep`) that prevent building.

We use a custom MMIO driver with direct register access:

| File | Purpose |
|------|---------|
| `drivers/i2c_hal.c` | Custom MMIO I2C driver (direct register access) |
| `drivers/i2c_hal.h` | I2C interface header |
| `drivers/pca9685.c` | PCA9685 servo driver |

### I2C-1 Hardware Configuration

```c
#define I2C1_BASE   0x04010000   // I2C1 register base address

// Pinmux configuration (function 3)
// GP4 = SCL
// GP5 = SDA
```

---

## Code Placement

Copy muscle runtime files to the SDK:

```bash
SDK_TASK=$SDK_DIR/freertos/cvitek/task/comm/src/riscv64

# Copy main application
cp app_main.c $SDK_TASK/

# Copy custom drivers
mkdir -p $SDK_TASK/drivers
cp -r drivers/* $SDK_TASK/drivers/

# Copy safety module
mkdir -p $SDK_TASK/safety
cp -r safety/* $SDK_TASK/safety/

# Copy motion runtime
cp motion_task.c motion_task.h $SDK_TASK/
cp rpmsg_motion_rx.c rpmsg_motion_rx.h $SDK_TASK/
```

---

## Build Commands

```bash
cd ~/duo-sdk-v1
./build.sh milkv-duo256m-sd

# Output: fsbl/build/cv1812cp_milkv_duo256m_sd/fip.bin (~405KB)
```

### Rebuild Cycle (Fast Iteration)

```bash
cd ~/duo-sdk-v1

# Clean only FreeRTOS
rm -rf freertos/cvitek/build

# Rebuild
./build.sh milkv-duo256m-sd
```

---

## Deploy

```bash
scp fip.bin root@192.168.42.1:/boot/
ssh root@192.168.42.1 reboot
```

Or from SDK directory:

```bash
scp ~/duo-sdk-v1/fsbl/build/cv1812cp_milkv_duo256m_sd/fip.bin \
    root@192.168.42.1:/boot/
ssh root@192.168.42.1 "sync && reboot"
```

---

## Verify

After reboot, check the following:

### 1. Blue LED Should Blink

The onboard blue LED blinks to indicate FreeRTOS is running.

### 2. Mailbox Device Exists

```bash
ls /dev/cvi-rtos-cmdqu
```

### 3. Serial Console Output

Connect to serial console and look for:

```
[Spider] Muscle Runtime v3.1 Starting
RT: I2C MMIO init OK
RT: PCA9685 init OK at 0x40
RT: Mailbox handler registered
```

---

## Inter-Core Communication

### Mailbox Device
- Linux: `/dev/cvi-rtos-cmdqu`
- FreeRTOS: IRQ handler in `rpmsg_motion_rx.c`

### Shared Memory
- Address: `0x83F00000`
- Size: 256KB
- Usage: Ring buffer for motion packets

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Build fails on HAL I2C | Apply `patches/hal_cmakelists.txt` |
| fip.bin won't boot | Ensure using SDK v1, not v2 |
| fip.bin too large (>1MB) | Wrong SDK version, use v1 |
| I2C read/write fails | Check GP4/GP5 pinmux configuration |
| No mailbox IRQ | Verify fip.bin deployed and rebooted |
| PCA9685 not found | Check I2C wiring, run `i2cdetect -y 1` |
| Blue LED not blinking | FreeRTOS not running, check fip.bin |

---

## References

- [Milk-V Duo FreeRTOS Core](https://milkv.io/docs/duo/getting-started/rtoscore)
- [Mailbox Examples](https://github.com/milkv-duo/duo-examples/tree/main/mailbox-test)
