#!/bin/bash
# Enable I2C support in SDK v1 for FreeRTOS (riscv64 small core)
# Run this from WSL before building

set -e

SDK_DIR="${HOME}/duo-sdk-v1"
HAL_DIR="${SDK_DIR}/freertos/cvitek/hal/cv181x"
DRIVER_DIR="${SDK_DIR}/freertos/cvitek/driver"
TASK_DIR="${SDK_DIR}/freertos/cvitek/task/comm"

echo "=== Enabling I2C support for Spider Robot v3.1 ==="

# 1. Enable I2C in HAL CMakeLists.txt
echo "[1/4] Patching HAL CMakeLists.txt to enable i2c..."
HAL_CMAKE="${HAL_DIR}/CMakeLists.txt"

if grep -q "# i2c" "$HAL_CMAKE"; then
    sed -i 's/# i2c/i2c/' "$HAL_CMAKE"
    echo "  -> Uncommented i2c in HAL build list"
else
    echo "  -> i2c already enabled or different format"
fi

# Verify
if grep -q "^[[:space:]]*i2c$" "$HAL_CMAKE" || grep -q '"i2c"' "$HAL_CMAKE"; then
    echo "  -> HAL i2c: ENABLED"
else
    echo "  -> WARNING: Could not verify i2c is enabled in HAL"
    cat "$HAL_CMAKE"
fi

# 2. Enable I2C in driver build for riscv64
echo "[2/4] Patching driver CMakeLists.txt to include i2c for riscv64..."
DRIVER_CMAKE="${DRIVER_DIR}/CMakeLists.txt"

# Check if i2c is already in the riscv64 list
if grep -A 10 'RUN_ARCH STREQUAL "riscv64"' "$DRIVER_CMAKE" | grep -q "i2c"; then
    echo "  -> i2c already in riscv64 driver list"
else
    # Add i2c to the riscv64 driver list
    sed -i '/RUN_ARCH STREQUAL "riscv64"/,/^else()/{ 
        /gpio$/a\        i2c
    }' "$DRIVER_CMAKE"
    echo "  -> Added i2c to riscv64 driver list"
fi

# 3. Install I2C headers to the right place
echo "[3/4] Ensuring I2C headers are available..."
mkdir -p "${SDK_DIR}/freertos/cvitek/install/include/driver/i2c"
mkdir -p "${SDK_DIR}/freertos/cvitek/install/include/hal/i2c"

cp -f "${DRIVER_DIR}/i2c/include/"*.h "${SDK_DIR}/freertos/cvitek/install/include/driver/i2c/" 2>/dev/null || true
cp -f "${HAL_DIR}/i2c/include/"*.h "${SDK_DIR}/freertos/cvitek/install/include/hal/i2c/" 2>/dev/null || true
echo "  -> Headers copied"

# 4. Show result
echo "[4/4] Verification:"
echo "  HAL CMakeLists.txt (build_list):"
grep -A 5 "set(build_list" "$HAL_CMAKE" | head -8
echo ""
echo "  Driver CMakeLists.txt (riscv64 section):"
grep -A 12 'RUN_ARCH STREQUAL "riscv64"' "$DRIVER_CMAKE" | head -15

echo ""
echo "=== Done! Now run: cd ~/duo-sdk-v1 && ./build.sh milkv-duo256m-sd ==="
