#!/bin/bash
# Build script for Spider Robot v3.1 Brain Daemon
# Run from WSL with Milk-V SDK installed

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
TOOLCHAIN_FILE="${SCRIPT_DIR}/toolchain-milkv-duo.cmake"

# Default toolchain path
TOOLCHAIN_PATH="${TOOLCHAIN_PATH:-$HOME/duo-sdk-v1/host-tools/gcc/riscv64-linux-musl-x86_64}"

echo "=== Spider Robot v3.1 Brain Daemon Build ==="
echo "Toolchain: ${TOOLCHAIN_PATH}"
echo "Build dir: ${BUILD_DIR}"

# Check toolchain exists
if [ ! -x "${TOOLCHAIN_PATH}/bin/riscv64-unknown-linux-musl-gcc" ]; then
    echo "ERROR: Toolchain not found at ${TOOLCHAIN_PATH}"
    echo "Please set TOOLCHAIN_PATH or install SDK to ~/duo-sdk-v1/"
    exit 1
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
echo ""
echo "=== Configuring ==="
cmake \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DTOOLCHAIN_PATH="${TOOLCHAIN_PATH}" \
    -DCMAKE_BUILD_TYPE=Release \
    "${SCRIPT_DIR}"

# Build
echo ""
echo "=== Building ==="
cmake --build . -j$(nproc)

# Show result
echo ""
echo "=== Build Complete ==="
ls -la brain_daemon

# Strip binary for smaller size
${TOOLCHAIN_PATH}/bin/riscv64-unknown-linux-musl-strip brain_daemon
echo "Stripped binary:"
ls -la brain_daemon

echo ""
echo "Deploy to Milk-V Duo:"
echo "  scp build/brain_daemon root@192.168.42.1:/root/"
echo "  ssh root@192.168.42.1 'chmod +x /root/brain_daemon && /root/brain_daemon'"
