# 🕷️ Spider Robot v3.1

**Quadruped robot (4 legs × 3 DOF) powered by Milk-V Duo 256M dual-core architecture**

![Version](https://img.shields.io/badge/version-3.1-blue)
![FreeRTOS](https://img.shields.io/badge/Muscle-FreeRTOS-green)
![Linux](https://img.shields.io/badge/Brain-Linux-orange)

---

## Features

### Dual-Core Architecture
- **FreeRTOS Muscle** — Real-time servo control on RISC-V small core
- **Linux Brain** — High-level control, WebSocket API, sensor fusion on main core

### Hardware
- **12-DOF Legs** — 4 legs × 3 joints (coxa, femur, tibia)
- **Dual Animated Eyes** — GC9D01 round displays with mood animations
- **VL53L0X Distance Sensor** — Time-of-flight obstacle detection
- **Scan Servo (CH12)** — Sweeping sensor for navigation

### Software
- **WebSocket Control API** — Port 9000, JSON protocol
- **Serial Control** — /dev/ttyS0 @ 115200 for microcontrollers
- **Web Control UI** — Browser-based control panel (control_ui.html)
- **Walking Gaits** — Tripod (fast), Wave (stable)
- **Autonomous Navigation** — Obstacle avoidance with scan servo
- **Eye Moods** — Normal, Angry, Happy, Sleepy, Blink

---

## Architecture

```
┌─────────────────┐
│ External Client │  (Python/Web)
└────────┬────────┘
         │ WebSocket :9000
         ▼
┌─────────────────┐      Unix Socket       ┌─────────────────┐
│  Brain Daemon   │◄─────────────────────►│   Eye Service   │
│    (Linux)      │   /tmp/spider_eye.sock │    (Linux)      │
└────────┬────────┘                        └────────┬────────┘
         │ Mailbox                                  │ SPI
         │ /dev/cvi-rtos-cmdqu                      ▼
         ▼                                 ┌─────────────────┐
┌─────────────────┐                        │  GC9D01 x2      │
│ Muscle Runtime  │                        │  (Dual Eyes)    │
│   (FreeRTOS)    │                        └─────────────────┘
└────────┬────────┘
         │ I2C
         ▼
┌─────────────────┐
│    PCA9685      │
│ (16-ch PWM)     │
└────────┬────────┘
         │ PWM
         ▼
┌─────────────────┐
│   12 Servos     │
│  + Scan Servo   │
└─────────────────┘
```

---

## Quick Start

See [QUICKSTART.md](QUICKSTART.md) for full setup instructions.

```bash
# Deploy to Spider
scp fip_working.bin root@192.168.42.1:/boot/fip.bin
scp brain_daemon eye_service root@192.168.42.1:/root/

# Reboot (services auto-start via S99spider)
ssh root@192.168.42.1 "sync && reboot"

# Control via Web UI
# Open python/control_ui.html in browser
# Connect to ws://192.168.42.1:9000

# Or via Python
python python/demo_live.py
```

---

## Directory Structure

```
v3.1/
├── brain_linux/        # Linux daemons (Brain + Eye Service)
│   ├── src/            # Brain Daemon (WebSocket :9000, Serial, Mailbox) ← CANONICAL
│   └── eye_service/    # GC9D01 display driver, animations
├── muscle_rtos/        # FreeRTOS firmware
│   └── src/            # I2C driver, PCA9685, mailbox handler
├── common/             # Shared headers (protocols, packets)
├── python/             # Client library & demos
│   ├── spider_client.py      # WebSocket client
│   ├── control_ui.html       # Web control panel
│   ├── gait_library.py       # Walking gaits
│   ├── obstacle_avoidance.py # Navigation module
│   └── demo_autonomous.py    # Autonomous demo
├── docs/               # Documentation
├── tests/              # Test scripts
├── deploy/             # Deployment scripts & configs
├── fip.bin             # Compiled FreeRTOS firmware
├── QUICKSTART.md       # Getting started guide
└── STATUS.md           # Feature status tracking
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

Please ensure code compiles and follows existing style conventions.
