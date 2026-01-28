#!/bin/bash
#
# Spider Robot v3.1 - Build and Deploy Script
#
# Run from WSL: wsl -e bash build_and_deploy.sh
#
# Usage:
#   ./build_and_deploy.sh              # Build and deploy all
#   ./build_and_deploy.sh freertos     # Build and deploy FreeRTOS only
#   ./build_and_deploy.sh linux        # Build and deploy Linux daemons only
#   ./build_and_deploy.sh test         # Run tests after deploy
#

set -e

# Configuration
SDK_DIR="${SDK_DIR:-$HOME/duo-sdk-v1}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DUO_IP="${DUO_IP:-192.168.42.1}"
DUO_USER="root"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_sdk() {
    if [ ! -d "$SDK_DIR" ]; then
        log_error "SDK not found at $SDK_DIR"
        log_info "Clone SDK with: git clone https://github.com/milkv-duo/duo-buildroot-sdk.git --depth=1 $SDK_DIR"
        exit 1
    fi
    log_info "SDK found at $SDK_DIR"
}

copy_sources_to_sdk() {
    log_info "Copying muscle runtime sources to SDK..."
    
    SDK_TASK="$SDK_DIR/freertos/cvitek/task/comm/src/riscv64"
    
    # Copy main application
    cp "$SCRIPT_DIR/muscle_rtos/app_main_v1.c" "$SDK_TASK/comm_main.c"
    
    # Copy drivers
    mkdir -p "$SDK_TASK/drivers"
    cp "$SCRIPT_DIR/muscle_rtos/drivers/"*.c "$SDK_TASK/drivers/" 2>/dev/null || true
    cp "$SCRIPT_DIR/muscle_rtos/drivers/"*.h "$SDK_TASK/drivers/" 2>/dev/null || true
    
    # Copy safety module
    mkdir -p "$SDK_TASK/safety"
    cp "$SCRIPT_DIR/muscle_rtos/safety/"*.c "$SDK_TASK/safety/" 2>/dev/null || true
    cp "$SCRIPT_DIR/muscle_rtos/safety/"*.h "$SDK_TASK/safety/" 2>/dev/null || true
    
    # Copy common headers
    cp "$SCRIPT_DIR/common/"*.h "$SDK_TASK/" 2>/dev/null || true
    cp "$SCRIPT_DIR/common/"*.c "$SDK_TASK/" 2>/dev/null || true
    
    log_info "Sources copied to SDK"
}

build_freertos() {
    log_info "Building FreeRTOS Muscle Runtime..."
    
    check_sdk
    copy_sources_to_sdk
    
    cd "$SDK_DIR"
    
    # Clean previous build (optional, faster iteration)
    # rm -rf freertos/cvitek/build
    
    # Build
    ./build.sh milkv-duo256m-sd
    
    FIP_BIN="$SDK_DIR/fsbl/build/cv1812cp_milkv_duo256m_sd/fip.bin"
    if [ -f "$FIP_BIN" ]; then
        FIP_SIZE=$(stat -f%z "$FIP_BIN" 2>/dev/null || stat -c%s "$FIP_BIN" 2>/dev/null)
        log_info "Build successful: $FIP_BIN (${FIP_SIZE} bytes)"
        
        # Copy to project directory
        cp "$FIP_BIN" "$SCRIPT_DIR/fip.bin"
        log_info "Copied to $SCRIPT_DIR/fip.bin"
    else
        log_error "Build failed - fip.bin not found"
        exit 1
    fi
}

build_linux() {
    log_info "Building Linux daemons..."
    
    BUILD_DIR="$SCRIPT_DIR/build"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with cross-compiler if available
    if [ -f "$SCRIPT_DIR/toolchain-milkv-duo.cmake" ]; then
        cmake -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/toolchain-milkv-duo.cmake" ..
    else
        cmake ..
    fi
    
    make -j$(nproc)
    
    log_info "Build complete:"
    # Binaries are in brain_linux/src/ and brain_linux/eye_service/ subdirs
    find "$BUILD_DIR" -type f \( -name "brain_daemon" -o -name "eye_service" \) -executable 2>/dev/null | head -5
}

deploy_freertos() {
    log_info "Deploying FreeRTOS to $DUO_IP..."
    
    FIP_BIN="$SCRIPT_DIR/fip.bin"
    if [ ! -f "$FIP_BIN" ]; then
        log_error "fip.bin not found. Run build first."
        exit 1
    fi
    
    scp "$FIP_BIN" "${DUO_USER}@${DUO_IP}:/boot/"
    log_info "fip.bin deployed to /boot/"
    
    log_warn "Rebooting Duo to apply new firmware..."
    ssh "${DUO_USER}@${DUO_IP}" "sync && reboot" || true
    
    log_info "Waiting for reboot (30s)..."
    sleep 30
    
    # Wait for Duo to come back online
    for i in {1..10}; do
        if ping -c 1 -W 1 "$DUO_IP" >/dev/null 2>&1; then
            log_info "Duo is back online!"
            return 0
        fi
        sleep 2
    done
    
    log_warn "Duo may still be rebooting. Check connection manually."
}

deploy_linux() {
    log_info "Deploying Linux daemons to $DUO_IP..."
    
    BUILD_DIR="$SCRIPT_DIR/build"
    
    # Find binaries
    BRAIN_BIN=$(find "$BUILD_DIR" -name "brain_daemon" -type f | head -1)
    EYE_BIN=$(find "$BUILD_DIR" -name "eye_service" -type f | head -1)
    
    if [ -z "$BRAIN_BIN" ] || [ -z "$EYE_BIN" ]; then
        log_error "Binaries not found. Run build first."
        exit 1
    fi
    
    scp "$BRAIN_BIN" "$EYE_BIN" "${DUO_USER}@${DUO_IP}:/root/"
    log_info "Daemons deployed to /root/"
    
    # Kill existing processes and restart
    ssh "${DUO_USER}@${DUO_IP}" "killall brain_daemon eye_service 2>/dev/null; sleep 1; cd /root && ./brain_daemon &"
    log_info "brain_daemon started"
}

run_tests() {
    log_info "Running connectivity tests..."
    
    # Check if Duo is reachable
    if ! ping -c 1 -W 2 "$DUO_IP" >/dev/null 2>&1; then
        log_error "Cannot reach $DUO_IP"
        exit 1
    fi
    
    # Check mailbox device
    log_info "Checking mailbox device..."
    ssh "${DUO_USER}@${DUO_IP}" "ls -la /dev/cvi-rtos-cmdqu"
    
    # Check I2C
    log_info "Scanning I2C-1 for PCA9685..."
    ssh "${DUO_USER}@${DUO_IP}" "i2cdetect -y 1" || log_warn "i2cdetect not available"
    
    # Check processes
    log_info "Checking running processes..."
    ssh "${DUO_USER}@${DUO_IP}" "ps | grep -E '(brain_daemon|eye_service)'" || log_warn "Daemons not running"
    
    log_info "Tests complete!"
}

show_help() {
    echo "Spider Robot v3.1 - Build and Deploy"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  all       Build and deploy everything (default)"
    echo "  freertos  Build and deploy FreeRTOS muscle runtime"
    echo "  linux     Build and deploy Linux daemons"
    echo "  build     Build only (no deploy)"
    echo "  deploy    Deploy only (no build)"
    echo "  test      Run connectivity tests"
    echo "  help      Show this help"
    echo ""
    echo "Environment:"
    echo "  SDK_DIR   Path to duo-sdk-v1 (default: ~/duo-sdk-v1)"
    echo "  DUO_IP    Milk-V Duo IP address (default: 192.168.42.1)"
}

# Main
case "${1:-all}" in
    freertos)
        build_freertos
        deploy_freertos
        ;;
    linux)
        build_linux
        deploy_linux
        ;;
    build)
        build_freertos
        build_linux
        ;;
    deploy)
        deploy_freertos
        deploy_linux
        ;;
    test)
        run_tests
        ;;
    all)
        build_freertos
        build_linux
        deploy_freertos
        deploy_linux
        run_tests
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        log_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac

log_info "Done!"
