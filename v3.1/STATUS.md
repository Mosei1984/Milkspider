# Spider Robot v3.1 - Project Status

**Last Updated:** 2026-01-28  
**Build Status:** ✅ Firmware deployed and tested on Milk-V Duo 256M

---

## System Architecture

```
┌─────────────────┐     WebSocket      ┌─────────────────────────────────────┐
│  Python Client  │◄──────:9000───────►│          Milk-V Duo 256M            │
│  (PC/External)  │                    │                                     │
└─────────────────┘                    │  ┌─────────────┐  ┌─────────────┐   │
                                       │  │Brain Daemon │  │ Eye Service │   │
┌─────────────────┐     Serial         │  │  (Linux)    │◄►│  (Linux)    │   │
│ Microcontroller │◄───/dev/ttyS0─────►│  └──────┬──────┘  └──────┬──────┘   │
│  (ESP32/etc.)   │    115200 baud     │         │                 │         │
└─────────────────┘                    │    Mailbox+SharedMem     SPI        │
                                       │         │                 │         │
                                       │  ┌──────▼──────┐  ┌──────▼──────┐   │
                                       │  │   Muscle    │  │  GC9D01 x2  │   │
                                       │  │ (FreeRTOS)  │  │   Displays  │   │
                                       │  └──────┬──────┘  └─────────────┘   │
                                       │         │ I2C                       │
                                       │  ┌──────▼──────┐                    │
                                       │  │   PCA9685   │                    │
                                       │  │  13 Servos  │                    │
                                       │  └─────────────┘                    │
                                       └─────────────────────────────────────┘
```

---

## Working Features

### FreeRTOS Muscle Runtime (Small Core)
- [x] Custom MMIO I2C driver (SDK HAL is broken)
- [x] PCA9685 16-channel PWM driver
- [x] Mailbox command handler (CMD_MOTION_PACKET, CMD_ESTOP, CMD_HEARTBEAT)
- [x] Shared memory ring buffer at `0x83F00000`
- [x] CRC16 packet validation
- [x] Watchdog + Failsafe system
- [x] Deployed as `fip_working.bin` (~401KB)

### Brain Daemon (Linux Big Core)
- [x] WebSocket server on port 9000
- [x] **Serial interface on /dev/ttyS0 @ 115200** (NEW)
- [x] Commands: servo, servos, move, scan, estop, resume, status, get_servos
- [x] Mailbox communication via `/dev/cvi-rtos-cmdqu`
- [x] Shared memory writer for PosePacket31 (42 bytes with CRC16)
- [x] Structured logging with timestamps
- [x] Eye Service reconnection on disconnect
- [x] Statistics logging (uptime, packets, clients)

### Eye Service (Linux Big Core)
- [x] GC9D01 dual-display SPI driver
- [x] GPIO sysfs backend for CS/DC/RST pins
- [x] Eye renderer with moods: Normal, Angry, Happy, Sleepy
- [x] Eye animator with idle behaviors
- [x] Boot animation sequence
- [x] Unix socket IPC at `/tmp/spider_eye.sock`

### Python Control Library
- [x] `spider_client.py` - WebSocket client with full API
- [x] `calibrated_client.py` - Auto-applies servo calibration
- [x] `gait_library.py` - Tripod & Wave walking gaits
- [x] `obstacle_avoidance.py` - Scan-based navigation module
- [x] `control_ui.html` - Web-based control interface

### Control Interfaces

| Interface | Protocol | Use Case |
|-----------|----------|----------|
| WebSocket | JSON over ws://IP:9000 | PC control, debugging |
| Serial | Text commands /dev/ttyS0 | Microcontroller (ESP32, Arduino) |
| Web UI | HTML/JS (control_ui.html) | Browser-based control panel |

---

## Serial Command Reference

```
HELP                              List all commands
STATUS                            System status
SERVO <ch> <us>                   Set single servo (ch=0-12, us=500-2500)
SERVOS <us0> <us1> ... <us12>     Set all 13 servos
MOVE <t_ms> <us0> ... <us12>      Interpolated move
SCAN <us>                         Scan servo position (us=500-2500)
ESTOP                             Emergency stop
RESUME                            Clear E-STOP
EYE MOOD <normal|angry|happy|sleepy>
EYE LOOK <x> <y>                  Look direction (-1.0 to 1.0)
EYE BLINK
EYE WINK <left|right>
DISTANCE                          Read distance sensor (mm)
```

Response: `OK [value]` or `ERR <message>`

---

## WebSocket Command Reference

| Command | Request | Response |
|---------|---------|----------|
| Status | `{"type":"status"}` | `{"status":"ok","seq":N,...}` |
| Single Servo | `{"type":"servo","channel":0,"us":1500}` | `{"status":"ok","channel":0,"us":1500}` |
| All Servos | `{"type":"servos","us":[...13 values...]}` | `{"status":"ok","count":13}` |
| Move | `{"type":"move","t_ms":100,"us":[...]}` | `{"status":"ok","t_ms":100,"seq":N}` |
| Scan | `{"type":"scan","us":1500}` | `{"status":"ok","scan_us":1500}` |
| E-STOP | `{"type":"estop"}` | `{"status":"estop_activated"}` |
| Distance | `{"type":"distance"}` | `{"distance_mm":123,"status":"ok"}` |
| Eye Look | `{"type":"look","x":0.5,"y":-0.3}` | `{"status":"ok","eye":"look"}` |
| Eye Mood | `{"type":"mood","mood":"happy"}` | `{"status":"ok","eye":"mood"}` |
| Eye Blink | `{"type":"blink"}` | `{"status":"ok","eye":"blink"}` |

---

## Web Control UI

Open `python/control_ui.html` in any browser.

**Features:**
- E-Stop / Resume
- Eye control (moods, look direction, blink/wink)
- Scan servo slider (0-180°)
- Distance sensor readout
- All 12 leg servo sliders
- Walking controls (Tripod/Wave gait, direction pad, speed)
- Command log (TX/RX)
- Custom JSON command input

**Usage:**
1. Open `control_ui.html` in browser
2. Enter WebSocket URL: `ws://192.168.42.1:9000`
3. Click **Connect**
4. Use controls

---

## Obstacle Avoidance Module

```python
from obstacle_avoidance import ObstacleAvoidance, Action
from spider_client import SpiderClient

client = SpiderClient()
client.connect()

oa = ObstacleAvoidance(client)
oa.integrate_with_eyes()  # Eyes follow scan direction

action = oa.get_recommended_action()
if action == Action.FORWARD:
    # Path clear, move forward
elif action == Action.TURN_LEFT:
    # Better clearance on left
elif action == Action.BACKUP:
    # Too close, reverse
```

---

## File Structure

```
v3.1/
├── brain_linux/
│   ├── src/                    # Brain Daemon source (main implementation)
│   │   ├── main.cpp            # WebSocket + Serial server
│   │   ├── serial_control.cpp  # Serial command interface
│   │   ├── mailbox.cpp         # RTOS mailbox communication
│   │   ├── shared_memory.cpp   # Ring buffer for motion packets
│   │   ├── eye_client.cpp      # Eye Service client
│   │   ├── distance_sensor.cpp # VL53L0X driver
│   │   └── logger.h            # Structured logging
│   └── eye_service/            # Eye animation service
├── muscle_rtos/                # FreeRTOS small core firmware
│   ├── app_main_v1.c           # Main entry point
│   ├── drivers/                # I2C, PCA9685 drivers
│   └── safety/                 # Watchdog, failsafe
├── common/                     # Shared headers
│   ├── protocol_posepacket31.h # Motion packet format
│   ├── limits.h                # Servo limits, channel mapping
│   └── crc16_ccitt_false.c     # CRC implementation
├── python/
│   ├── spider_client.py        # WebSocket client library
│   ├── calibrated_client.py    # Client with calibration
│   ├── gait_library.py         # Walking gaits
│   ├── obstacle_avoidance.py   # Scan-based obstacle avoidance
│   ├── calibration.py          # Interactive calibration tool
│   ├── control_ui.html         # Web-based control panel
│   ├── demo_autonomous.py      # Autonomous navigation demo
│   ├── demo_live.py            # Live system test
│   ├── test_hardware.py        # Hardware test suite
│   └── test_serial.py          # Serial command tester
├── deploy/
│   ├── S99spider               # BusyBox init script (autostart)
│   └── install_services.sh     # Service installation
├── docs/
│   └── WIRING.md               # Hardware wiring guide
├── fip_working.bin             # Working FreeRTOS firmware
├── QUICKSTART.md               # Getting started guide
└── README.md                   # Project overview
```

---

## Hardware Pinout

| Function | Pin | Notes |
|----------|-----|-------|
| **I2C1 (PCA9685)** | | |
| SCL | GP4 | Servo driver |
| SDA | GP5 | Address 0x40 |
| **I2C2 (VL53L0X)** | | |
| SCL | GP2 | Distance sensor |
| SDA | GP3 | Address 0x29 |
| **SPI2 (Displays)** | | |
| SCLK | GP6 | /dev/spidev0.0 |
| MOSI | GP7 | |
| DC | GP8 | GPIO 506 |
| CS_LEFT | J2-25 | GPIO 505 |
| CS_RIGHT | J2-26 | GPIO 507 |
| RST_LEFT | J2-31 | GPIO 451 |
| RST_RIGHT | J2-32 | GPIO 454 |
| **Serial** | | |
| TX | UART0 TX | /dev/ttyS0 |
| RX | UART0 RX | 115200 baud |

---

## Servo Channel Mapping

| Channel | Function | Leg |
|---------|----------|-----|
| 0 | Coxa | Front Right (FR) |
| 1 | Femur | Front Right |
| 2 | Tibia | Front Right |
| 3 | Coxa | Front Left (FL) |
| 4 | Femur | Front Left |
| 5 | Tibia | Front Left |
| 6 | Coxa | Rear Right (RR) |
| 7 | Femur | Rear Right |
| 8 | Tibia | Rear Right |
| 9 | Coxa | Rear Left (RL) |
| 10 | Femur | Rear Left |
| 11 | Tibia | Rear Left |
| **12** | **Scan Servo** | Head/Sensor mount |

---

## Deployment

```bash
# Build (from WSL)
cd v3.1/brain_linux/src
mkdir build && cd build
export MILKV_SDK_PATH=/opt/milkv/duo-sdk/gcc/riscv64-linux-musl-x86_64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-milkv-duo.cmake
make -j4

# Deploy (default password: milkv, or use SSH keys)
export DUO_IP=192.168.42.1
scp build/brain_daemon root@$DUO_IP:/root/
scp fip_working.bin root@$DUO_IP:/boot/fip.bin
ssh root@$DUO_IP "sync && reboot"

# After reboot, services auto-start via /etc/init.d/S99spider
```

---

## Next Steps (Hardware Required)

1. [ ] Wire VL53L0X distance sensor (I2C2)
2. [ ] Wire GC9D01 dual displays (SPI2)
3. [ ] Connect PCA9685 + servos (when delivered)
4. [ ] Run `python test_hardware.py`
5. [ ] Calibrate servos with `python calibration.py`
6. [ ] Test walking with `python demo_walk.py`
7. [ ] Test autonomous mode with `python demo_autonomous.py`

---

## Tested & Verified

- [x] Brain Daemon WebSocket communication (from PC)
- [x] Eye commands (mood, look, blink, wink) via WebSocket
- [x] Scan servo commands via WebSocket
- [x] Motion packet transmission to RTOS core
- [x] Mailbox communication (no buffer errors)
- [x] Auto-start on boot via S99spider
- [x] Cross-compilation for RISC-V with Milk-V SDK
- [x] Serial control interface (/dev/ttyS0 @ 115200)
- [x] Web Control UI (control_ui.html)
- [x] Walking gait commands via UI
