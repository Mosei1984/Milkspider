# Spider Robot v3.1 - Quickstart Guide

**Platform:** Milk-V Duo 256M  
**Architecture:** FreeRTOS (Muscle) + Linux (Brain + Eye)

---

## 1. Prerequisites

### Hardware
- **Milk-V Duo 256M** with duo-sdk-v1 installed
- **PCA9685** 16-channel PWM servo driver
- **12x Servos** (SG90 or MG90S recommended)
- **VL53L0X** distance sensor (optional)
- **2x GC9D01** round TFT displays for eyes (optional)
- **5-6V Power Supply** (4A+ for servos)

### Host System
- USB RNDIS connection to Duo at `192.168.42.1`
- Python 3.8+ with `websocket-client`:
  ```bash
  pip install websocket-client
  ```
- WSL or Linux for building (cross-compilation)

### SDK Setup
```bash
git clone https://github.com/milkv-duo/duo-buildroot-sdk.git --depth=1 ~/duo-sdk-v1
export SDK_DIR=~/duo-sdk-v1
```

---

## 2. Hardware Setup

### Quick Wiring Checklist

| Component | Interface | Pins | Address |
|-----------|-----------|------|---------|
| PCA9685 | I2C1 | GP4 (SCL), GP5 (SDA) | 0x40 |
| VL53L0X | I2C2 | I2C2_SCL, I2C2_SDA | 0x29 |
| Eye Displays | SPI2 | GP6 (SCLK), GP7 (MOSI), GP8 (DC) | - |

### Servo Channel Mapping
| CH | Function | CH | Function |
|----|----------|----|---------| 
| 0 | FR Coxa | 6 | RR Coxa |
| 1 | FR Femur | 7 | RR Femur |
| 2 | FR Tibia | 8 | RR Tibia |
| 3 | FL Coxa | 9 | RL Coxa |
| 4 | FL Femur | 10 | RL Femur |
| 5 | FL Tibia | 11 | RL Tibia |
| 12 | Scan Servo | | |

### Power Connections
```
USB-C (5V) ──► Milk-V Duo ──► 3.3V logic (sensors, PCA9685 VCC)
                                    │
External 5-6V (4A+) ──► PCA9685 V+ ─┴─► Servos
                            │
                        Common GND ◄── ALL DEVICES
```

⚠️ **CRITICAL:** Keep all GNDs connected! See [docs/WIRING.md](docs/WIRING.md) for full details.

### First Boot Verification
```bash
# Connect via USB RNDIS
ssh root@192.168.42.1

# Check I2C devices
i2cdetect -y -r 1   # Should show 0x40 (PCA9685)
i2cdetect -y -r 2   # Should show 0x29 (VL53L0X)

# Check mailbox device
ls -la /dev/cvi-rtos-cmdqu
```

---

## 3. Building

### Build FreeRTOS Muscle Runtime
```bash
# From WSL/Linux
cd v3.1
./build_and_deploy.sh freertos
```
This produces `fip.bin` (~405KB) containing the FreeRTOS firmware.

### Build Linux Daemons (Brain + Eye)
```bash
./build_and_deploy.sh linux
```
This produces:
- `build/brain_daemon` - WebSocket server for servo/sensor control
- `build/eye_service` - Eye animation service

### Build All
```bash
./build_and_deploy.sh build   # Build only (no deploy)
./build_and_deploy.sh all     # Build and deploy everything
```

### Environment Variables
```bash
export SDK_DIR=~/duo-sdk-v1       # Path to SDK (default: ~/duo-sdk-v1)
export DUO_IP=192.168.42.1        # Duo IP address (default: 192.168.42.1)
```

---

## 4. Deployment

### Deploy Firmware
```bash
# Deploy FreeRTOS firmware (requires reboot)
scp fip.bin root@192.168.42.1:/boot/
ssh root@192.168.42.1 "sync && reboot"
# Wait ~30s for reboot
```

### Deploy Daemons
```bash
scp build/brain_daemon build/eye_service root@192.168.42.1:/root/
```

### Install as systemd Services (Optional)
```bash
ssh root@192.168.42.1

# Create brain service
cat > /etc/init.d/S99brain << 'EOF'
#!/bin/sh
case "$1" in
  start) /root/brain_daemon &;;
  stop) killall brain_daemon;;
esac
EOF
chmod +x /etc/init.d/S99brain

# Create eye service (if using displays)
cat > /etc/init.d/S98eye << 'EOF'
#!/bin/sh
case "$1" in
  start) /root/eye_service &;;
  stop) killall eye_service;;
esac
EOF
chmod +x /etc/init.d/S98eye
```

### First Boot Procedure
```bash
ssh root@192.168.42.1

# Start services
./brain_daemon &
./eye_service &   # If using eye displays

# Verify WebSocket is listening
netstat -tln | grep 9000
```

---

## 5. Testing

### Run Hardware Test Suite
```bash
cd python
python test_hardware.py
```

### Test Options
```bash
python test_hardware.py --simulate       # Mock hardware (no robot needed)
python test_hardware.py --test-servos    # Servo tests only
python test_hardware.py --test-eyes      # Eye tests only
python test_hardware.py --test-distance  # Distance sensor only
```

### Expected Output (Full Test)
```
============================================================
SPIDER ROBOT v3.1 - HARDWARE TEST SUITE
============================================================
CONNECTION TEST
  Checking connection... OK
  Querying status... OK

SERVO TESTS
  [1/3] Setting all servos to neutral... OK
  [2/3] Testing individual leg servos...
    Leg 0 (FR): CH0 (coxa): OK, CH1 (femur): OK, CH2 (tibia): OK
    Leg 1 (FL): ...
  [3/3] Testing scan servo (CH12)... OK

EYE TESTS
  Mood cycle... OK
  Look directions... OK
  Blink/Wink... OK

DISTANCE SENSOR TESTS
  Single reading... 523mm OK
  Multi-sample... OK (avg: 518mm)

============================================================
TEST REPORT
  Total Tests: 24
  Passed:      24
  Failed:      0

  *** ALL TESTS PASSED ***
```

### Troubleshooting

| Problem | Check |
|---------|-------|
| Connection refused | Is `brain_daemon` running? |
| Servos not moving | I2C wiring, PCA9685 address 0x40 |
| Distance always 0 | VL53L0X wiring, I2C2 bus |
| Eyes blank | SPI wiring, GPIO sysfs numbers |

---

## 6. First Walk

### Step 1: Calibrate Servos
```bash
cd python
python calibration.py
```

Commands in calibration mode:
- `+5` / `-5` - Adjust offset by 5 degrees
- `=0` - Set offset to 0 degrees
- `c` - Center servo (90° + offset)
- `t` - Test sweep (60° → 90° → 120°)
- `s` - Save and continue to next channel
- `q` - Save and quit

Calibrate each leg:
```bash
python calibration.py --leg 0   # Front Right
python calibration.py --leg 1   # Front Left
python calibration.py --leg 2   # Rear Right
python calibration.py --leg 3   # Rear Left
```

### Step 2: Walking Demo
```bash
python demo_walk.py --cycles 3
```

This runs a tripod gait (diagonal legs move together).

### Step 3: Autonomous Demo
```bash
python demo_autonomous.py --duration 60
```

Uses distance sensor for obstacle avoidance. Add `--no-simulate` to use real sensor.

---

## 7. Command Reference

### WebSocket Commands (JSON over ws://192.168.42.1:9000)

| Command | Example | Description |
|---------|---------|-------------|
| `servo` | `{"type":"servo","channel":0,"us":1500}` | Set single servo |
| `servos` | `{"type":"servos","us":[1500,...]}` | Set all 12 servos |
| `get_servos` | `{"type":"get_servos"}` | Query servo positions |
| `estop` | `{"type":"estop"}` | Emergency stop |
| `stop` | `{"type":"stop"}` | Pause motion |
| `resume` | `{"type":"resume"}` | Resume motion |
| `status` | `{"type":"status"}` | Get system status |
| `distance` | `{"type":"distance"}` | Read VL53L0X |
| `scan` | `{"type":"scan","us":1500}` | Set scan servo |
| `look` | `{"type":"look","x":0.5,"y":0}` | Move eye pupils |
| `blink` | `{"type":"blink"}` | Trigger blink |
| `mood` | `{"type":"mood","mood":"happy"}` | Set eye mood |

### Python API Quick Reference

```python
from spider_client import SpiderClient

# Connect
client = SpiderClient(host="192.168.42.1", port=9000)
client.connect()

# Servo Control
client.set_servo(channel=0, us=1500)      # Single servo
client.set_all_servos([1500] * 12)        # All servos
client.all_neutral()                       # All to 1500us
client.estop()                             # Emergency stop

# Leg Control (coxa, femur, tibia)
client.set_leg(leg=0, coxa_us=1500, femur_us=1500, tibia_us=1500)

# Eye Control
client.eye_look(x=0.5, y=0)               # Move pupils (-1 to 1)
client.eye_blink()                         # Blink animation
client.eye_wink("left")                    # Wink left eye
client.eye_mood("happy")                   # normal/angry/happy/sleepy
client.eye_idle(True)                      # Enable idle animation

# Distance Sensor
dist_mm = client.get_distance()            # Returns int or None
is_close = client.is_obstacle_close(150)   # True if < 150mm

# Scan Servo (CH12)
client.scan_center()                       # 90°
client.scan_left(45)                       # 90° + 45°
client.scan_right(45)                      # 90° - 45°
client.scan_angle(120)                     # Specific angle
readings = client.scan_sweep()             # Sweep and read distances

# Cleanup
client.disconnect()
```

### Context Manager Usage
```python
with SpiderClient() as client:
    client.all_neutral()
    client.eye_mood("happy")
# Automatically disconnects
```

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│  SPIDER ROBOT v3.1 - QUICK REFERENCE                    │
├─────────────────────────────────────────────────────────┤
│  Build:    ./build_and_deploy.sh all                    │
│  Deploy:   scp fip.bin root@192.168.42.1:/boot/         │
│  Connect:  ws://192.168.42.1:9000                       │
├─────────────────────────────────────────────────────────┤
│  Test:     python test_hardware.py                      │
│  Calib:    python calibration.py                        │
│  Walk:     python demo_walk.py                          │
│  Auto:     python demo_autonomous.py                    │
├─────────────────────────────────────────────────────────┤
│  E-STOP:   {"type":"estop"} or client.estop()           │
└─────────────────────────────────────────────────────────┘
```

---

*For detailed wiring, see [docs/WIRING.md](docs/WIRING.md)*  
*For project status, see [STATUS.md](STATUS.md)*
