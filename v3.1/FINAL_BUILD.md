# Milkspider v3.1 - Final Build Instructions

**This is the single source of truth for building and deploying v3.1.**

## Prerequisites
- WSL2 with Milk-V SDK at `~/duo-sdk-v1`
- USB RNDIS connection to Duo at `192.168.42.1`

## Build & Deploy (One Command)
```bash
./build_and_deploy.sh linux
```

## Output Binaries
| Binary | Location | Function |
|--------|----------|----------|
| `brain_daemon` | `build/brain_linux/src/` | WebSocket :9000, Serial, IPC |
| `eye_service` | `build/brain_linux/eye_service/` | GC9D01 displays |

## Start on Duo
```bash
ssh root@192.168.42.1
nohup ./brain_daemon > /tmp/brain.log 2>&1 &
```

## Verify
```bash
python python/test_hardware.py
# Expected: 20/21 PASS (distance sensor = hardware pending)
```

## Canonical Entry Points
- **Brain:** `brain_linux/src/main.cpp`
- **Eye:** `brain_linux/eye_service/main.cpp`
- **Muscle:** `muscle_rtos/app_main_v1.c`

---
**Milkspider v3.1 is finalized.**
