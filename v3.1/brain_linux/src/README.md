# Spider Robot v3.1 - Brain Daemon

Linux-side daemon for the Milk-V Duo 256M that provides:
- WebSocket server (port 9000) for external control
- Mailbox communication with FreeRTOS small core via `/dev/cvi-rtos-cmdqu`
- Shared memory ring buffer at `0x83F00000` for PosePacket31 transfer

## Files

| File | Description |
|------|-------------|
| `main.cpp` | Main daemon with WebSocket server and command handling |
| `mailbox.cpp/.h` | CVITEK mailbox driver interface for Linux ↔ FreeRTOS IPC |
| `shared_memory.cpp/.h` | Physical memory mapping for PosePacket31 ring buffer |
| `sha1.h` | Minimal SHA-1 for WebSocket handshake (no OpenSSL dependency) |
| `CMakeLists.txt` | CMake build configuration |
| `toolchain-milkv-duo.cmake` | Cross-compile toolchain for RISC-V C906 |
| `build.sh` | Build script for WSL |

## Build Instructions

### Prerequisites

1. WSL (Windows Subsystem for Linux) with Ubuntu
2. Milk-V Duo SDK with RISC-V toolchain at `~/duo-sdk-v1/`

### Build Steps

```bash
# From WSL, navigate to project
cd /mnt/c/Users/mosei/Documents/PlatformIO/Projects/Milk-Spider/v3.1/brain_linux/src

# Make build script executable
chmod +x build.sh

# Build
./build.sh
```

### Manual Build

```bash
mkdir build && cd build

cmake \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-milkv-duo.cmake \
    -DTOOLCHAIN_PATH=$HOME/duo-sdk-v1/host-tools/gcc/riscv64-linux-musl-x86_64 \
    -DCMAKE_BUILD_TYPE=Release \
    ..

cmake --build . -j$(nproc)
```

## Deploy to Milk-V Duo

```bash
# Copy binary via RNDIS USB connection
scp build/brain_daemon root@192.168.42.1:/root/

# SSH and run
ssh root@192.168.42.1
chmod +x /root/brain_daemon
/root/brain_daemon
```

## WebSocket API (Port 9000)

### Commands (JSON)

```json
{"cmd": "estop"}              // Emergency stop
{"cmd": "stop"}               // Normal stop
{"cmd": "resume"}             // Clear E-STOP and resume
{"cmd": "status"}             // Get daemon status
{"type": "pose"}              // Send current servo positions
```

### Responses

```json
{"status": "connected", "version": "3.1"}
{"status": "estop_activated"}
{"status": "ok", "seq": 123, "tx_count": 456, "ring_w": 10, "ring_r": 8, "clients": 1}
{"error": "unknown_command"}
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Linux (C906 Big Core)                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │  WebSocket  │───▶│   Brain     │───▶│   Shared Memory     │  │
│  │  Clients    │    │   Daemon    │    │   0x83F00000        │  │
│  │  (Port 9000)│◀───│             │    │   PosePacket31[8]   │  │
│  └─────────────┘    └──────┬──────┘    └──────────┬──────────┘  │
│                            │                      │              │
│                    ┌───────▼───────┐              │              │
│                    │    Mailbox    │              │              │
│                    │ /dev/cvi-rtos │              │              │
│                    │   -cmdqu      │              │              │
│                    └───────┬───────┘              │              │
└────────────────────────────┼──────────────────────┼──────────────┘
                             │ CMD_MOTION_PACKET    │
                             ▼                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                   FreeRTOS (C906 Small Core)                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    Muscle Runtime                           ││
│  │  - Reads PosePacket31 from shared memory                    ││
│  │  - Interpolates servo positions                             ││
│  │  - Generates PWM signals (50Hz)                             ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

## Protocol Reference

- `PosePacket31`: 42-byte binary packet (see `common/protocol_posepacket31.h`)
- Mailbox `cmd_id`:
  - `0x20` CMD_MOTION_PACKET - New packet ready in ring buffer
  - `0x21` CMD_MOTION_ACK - Acknowledgment from RTOS
  - `0x22` CMD_HEARTBEAT - Keep-alive signal
  - `0x23` CMD_ESTOP - Emergency stop
