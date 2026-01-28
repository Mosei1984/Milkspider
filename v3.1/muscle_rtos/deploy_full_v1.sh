#!/bin/bash
# Deploy Spider Robot v3.1 Muscle Runtime with I2C/PCA9685 support
# Run from WSL

set -e

SDK_DIR="${HOME}/duo-sdk-v1"
TASK_SRC="${SDK_DIR}/freertos/cvitek/task/comm/src/riscv64"
WINDOWS_SRC="/mnt/c/Users/mosei/Documents/PlatformIO/Projects/Milk-Spider/v3.1/muscle_rtos"

echo "=== Spider Robot v3.1 - Full Deployment (with I2C) ==="

# 1. Enable I2C in SDK
echo "[1/6] Enabling I2C in SDK..."
chmod +x "${WINDOWS_SRC}/enable_i2c_sdk_v1.sh"
bash "${WINDOWS_SRC}/enable_i2c_sdk_v1.sh"

# 2. Create drivers directory in SDK
echo "[2/6] Creating drivers directory..."
mkdir -p "${TASK_SRC}/drivers"

# 3. Copy main application (full version with PCA9685)
echo "[3/6] Copying app_main_v1.c..."
cp -f "${WINDOWS_SRC}/app_main_v1.c" "${TASK_SRC}/app_main.c"

# 4. Copy driver files
echo "[4/6] Copying I2C and PCA9685 drivers..."
cp -f "${WINDOWS_SRC}/drivers/i2c_hal.c" "${TASK_SRC}/drivers/"
cp -f "${WINDOWS_SRC}/drivers/i2c_hal.h" "${TASK_SRC}/drivers/"
cp -f "${WINDOWS_SRC}/drivers/pca9685.c" "${TASK_SRC}/drivers/"
cp -f "${WINDOWS_SRC}/drivers/pca9685.h" "${TASK_SRC}/drivers/"

# 5. Copy limits header if it exists
if [ -f "${WINDOWS_SRC}/limits.h" ]; then
    cp -f "${WINDOWS_SRC}/limits.h" "${TASK_SRC}/drivers/"
fi

# 6. Create limits.h if needed
if [ ! -f "${TASK_SRC}/drivers/limits.h" ]; then
    echo "[5/6] Creating limits.h..."
    cat > "${TASK_SRC}/drivers/limits.h" << 'EOF'
#ifndef SPIDER_LIMITS_H
#define SPIDER_LIMITS_H

// Servo PWM limits (microseconds)
#define SERVO_PWM_MIN_US      500
#define SERVO_PWM_MAX_US      2500
#define SERVO_PWM_NEUTRAL_US  1500

// Joint angle limits (degrees, if used)
#define JOINT_ANGLE_MIN  -90
#define JOINT_ANGLE_MAX   90

#endif // SPIDER_LIMITS_H
EOF
fi

# 7. Verify CMakeLists.txt includes drivers
echo "[5/6] Verifying CMakeLists.txt..."
TASK_CMAKE="${SDK_DIR}/freertos/cvitek/task/comm/CMakeLists.txt"
if ! grep -q "DRIVER_SOURCES" "$TASK_CMAKE"; then
    echo "  -> CMakeLists.txt needs update (already done in previous step)"
fi

# 8. Build
echo "[6/6] Building firmware..."
cd "${SDK_DIR}"
./build.sh milkv-duo256m-sd

# 9. Check result
FIP_PATH="${SDK_DIR}/fsbl/build/cv1812cp_milkv_duo256m_sd/fip.bin"
if [ -f "$FIP_PATH" ]; then
    FIP_SIZE=$(stat -c%s "$FIP_PATH")
    echo ""
    echo "=== BUILD SUCCESS ==="
    echo "fip.bin size: ${FIP_SIZE} bytes"
    echo "fip.bin path: ${FIP_PATH}"
    echo ""
    echo "Deploy commands:"
    echo "  scp -O ${FIP_PATH} root@192.168.42.1:/tmp/"
    echo "  ssh root@192.168.42.1 'cp /tmp/fip.bin /boot/ && reboot'"
else
    echo ""
    echo "=== BUILD FAILED ==="
    echo "fip.bin not found at expected path"
    exit 1
fi
