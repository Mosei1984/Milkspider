# Spider Robot v3.1 - Hardware Wiring Guide

**Platform:** Milk-V Duo 256M  
**Last Updated:** 2026-01-28

---

## ⚠️ Safety Warnings

> **CRITICAL: Read before wiring!**

1. **NEVER connect/disconnect while powered** - Always power off before making wiring changes
2. **Double-check polarity** - Reversed power connections will destroy components
3. **Servo power must be separate** - Do NOT power servos from Milk-V Duo 3.3V rail
4. **Current limits** - Milk-V Duo GPIO pins are 3.3V, max ~8mA per pin
5. **Ground everything together** - All GND pins must be connected to a common ground
6. **Protect I2C lines** - Keep I2C wires short (<20cm) to avoid signal degradation

---

## 1. VL53L0X Distance Sensor (I2C2)

### Specifications
| Parameter | Value |
|-----------|-------|
| I2C Address | 0x29 (default) |
| Voltage | 2.6V - 3.5V (3.3V recommended) |
| Current | ~20mA active |
| Interface | I2C2 via `/dev/i2c-2` |
| Range | 30mm - 2000mm |

### Pin Connections

| VL53L0X Pin | Milk-V Duo Pin | Function | Wire Color |
|-------------|----------------|----------|------------|
| VCC | 3.3V (Pin 1) | Power | 🔴 Red |
| GND | GND (Pin 6) | Ground | ⚫ Black |
| SDA | I2C2_SDA | Data | 🔵 Blue |
| SCL | I2C2_SCL | Clock | 🟡 Yellow |
| XSHUT | NC | Shutdown (optional) | - |
| GPIO1 | NC | Interrupt (optional) | - |

### Wiring Diagram
```
    VL53L0X                      Milk-V Duo 256M
   ┌─────────┐                   ┌─────────────┐
   │  VCC ●──┼───── Red ────────►│ 3.3V (Pin 1)│
   │  GND ●──┼───── Black ──────►│ GND  (Pin 6)│
   │  SDA ●──┼───── Blue ───────►│ I2C2_SDA    │
   │  SCL ●──┼───── Yellow ─────►│ I2C2_SCL    │
   │XSHUT ●  │ (not connected)   │             │
   │GPIO1 ●  │ (not connected)   │             │
   └─────────┘                   └─────────────┘
```

### Notes
- The VL53L0X module typically includes onboard 2.8V regulator (accepts 3.3V-5V input)
- I2C pull-ups (4.7kΩ) are usually included on the breakout board
- Mount sensor at front of robot, pointing forward for obstacle detection

---

## 2. Dual GC9D01 Eye Displays (SPI2)

### Specifications
| Parameter | Value |
|-----------|-------|
| Display Type | GC9D01 Round TFT |
| Resolution | 240x240 pixels |
| Interface | SPI2 via `/dev/spidev0.0` |
| Voltage | 3.3V |
| SPI Speed | Up to 40MHz |

### Pin Connections

| Signal | Milk-V Pin | GPIO sysfs | Physical Pin | Function |
|--------|------------|------------|--------------|----------|
| SCLK | GP6 | - | J2-6 | SPI Clock |
| MOSI | GP7 | - | J2-7 | SPI Data Out |
| DC | GP8 | 506 | J2-8 | Data/Command Select |
| CS_LEFT | - | 505 | J2-25 | Left Eye Chip Select |
| CS_RIGHT | - | 507 | J2-26 | Right Eye Chip Select |
| RST_LEFT | - | 451 | J2-31 | Left Eye Reset |
| RST_RIGHT | - | 454 | J2-32 | Right Eye Reset |

### Power Connections (Both Displays)
| Display Pin | Connection | Notes |
|-------------|------------|-------|
| VCC | 3.3V | Both displays share 3.3V |
| GND | GND | Both displays share GND |
| BLK | 3.3V (or PWM) | Backlight - tie high for always-on |

### Wiring Diagram
```
                          Milk-V Duo 256M
                         ┌────────────────┐
                         │                │
   LEFT EYE (GC9D01)     │                │     RIGHT EYE (GC9D01)
  ┌─────────────┐        │                │        ┌─────────────┐
  │ VCC ●───────┼────────┤ 3.3V ◄────────┼────────┼──● VCC      │
  │ GND ●───────┼────────┤ GND  ◄────────┼────────┼──● GND      │
  │ SCL ●───────┼────────┤ GP6  (SCLK)───┼────────┼──● SCL      │
  │ SDA ●───────┼────────┤ GP7  (MOSI)───┼────────┼──● SDA      │
  │ DC  ●───────┼────────┤ GP8  (DC) ────┼────────┼──● DC       │
  │ CS  ●───────┼────────┤ J2-25 (CS_L)  │        │             │
  │ RST ●───────┼────────┤ J2-31 (RST_L) │        │             │
  │ BLK ●───3.3V┤        │               │        │             │
  └─────────────┘        │       (CS_R)  J2-26 ───┼──● CS       │
                         │       (RST_R) J2-32 ───┼──● RST      │
                         │                │   3.3V┼──● BLK      │
                         └────────────────┘        └─────────────┘

  Wire Legend:
  ═══ Power (3.3V, GND)
  ─── Signal (shared SPI bus)
  ─── Control (individual CS/RST)
```

### Signal Flow
```
                    ┌─────────────────────────────────────┐
                    │         SPI2 Bus (Shared)           │
                    │  SCLK ─────┬─────────────────────┐  │
                    │  MOSI ─────┼───────────────────┐ │  │
                    │  DC   ─────┼─────────────────┐ │ │  │
                    └────────────┼─────────────────┼─┼─┼──┘
                                 │                 │ │ │
                    ┌────────────▼─────┐   ┌───────▼─▼─▼────┐
                    │    LEFT EYE      │   │    RIGHT EYE   │
                    │  CS_LEFT (505)   │   │  CS_RIGHT (507)│
                    │  RST_LEFT (451)  │   │  RST_RIGHT(454)│
                    └──────────────────┘   └────────────────┘
```

### GPIO Control via sysfs
```bash
# Export GPIOs
echo 505 > /sys/class/gpio/export  # CS_LEFT
echo 506 > /sys/class/gpio/export  # DC
echo 507 > /sys/class/gpio/export  # CS_RIGHT
echo 451 > /sys/class/gpio/export  # RST_LEFT
echo 454 > /sys/class/gpio/export  # RST_RIGHT

# Set as outputs
echo out > /sys/class/gpio/gpio505/direction
echo out > /sys/class/gpio/gpio506/direction
echo out > /sys/class/gpio/gpio507/direction
echo out > /sys/class/gpio/gpio451/direction
echo out > /sys/class/gpio/gpio454/direction
```

---

## 3. PCA9685 Servo Driver (I2C1)

### Specifications
| Parameter | Value |
|-----------|-------|
| I2C Address | 0x40 (default) |
| Logic Voltage | 3.3V - 5V |
| Servo Voltage | 5V - 6V (external) |
| PWM Channels | 16 (CH0-CH15) |
| PWM Frequency | 50Hz (for servos) |
| Interface | I2C1 via custom MMIO driver |

### Pin Connections

| PCA9685 Pin | Milk-V Duo Pin | Function |
|-------------|----------------|----------|
| VCC | 3.3V (Pin 1) | Logic Power |
| GND | GND (Pin 6) | Ground |
| SCL | GP4 | I2C Clock |
| SDA | GP5 | I2C Data |
| V+ | External 5-6V | Servo Power |
| OE | GND or NC | Output Enable (active LOW) |

### Wiring Diagram
```
     PCA9685 Servo Driver                    Milk-V Duo 256M
    ┌─────────────────────┐                 ┌─────────────────┐
    │                     │                 │                 │
    │  VCC ●──────────────┼─── 3.3V ───────►│ Pin 1 (3.3V)    │
    │  GND ●──────────────┼─── GND ────────►│ Pin 6 (GND)     │
    │  SCL ●──────────────┼─────────────────│ GP4 (I2C1_SCL)  │
    │  SDA ●──────────────┼─────────────────│ GP5 (I2C1_SDA)  │
    │   OE ●──┐           │                 └─────────────────┘
    │         ▼ GND       │
    │  V+  ●──────────────┼─── 5-6V ◄───┐
    │                     │             │
    └──────┬──────────────┘      ┌──────┴──────────┐
           │                     │  External Power │
     ┌─────┴─────┐               │  Supply 5-6V    │
     │  Servos   │               │  (2-4A min)     │
     │ CH0-CH15  │               └─────────────────┘
     └───────────┘                      │
           GND ◄────────────────────────┘ (Common Ground!)
```

### Servo Channel Mapping

| Channel | Function | Servo Position |
|---------|----------|----------------|
| CH0 | Front-Left Coxa | Hip rotation |
| CH1 | Front-Left Femur | Upper leg |
| CH2 | Front-Left Tibia | Lower leg |
| CH3 | Front-Right Coxa | Hip rotation |
| CH4 | Front-Right Femur | Upper leg |
| CH5 | Front-Right Tibia | Lower leg |
| CH6 | Rear-Left Coxa | Hip rotation |
| CH7 | Rear-Left Femur | Upper leg |
| CH8 | Rear-Left Tibia | Lower leg |
| CH9 | Rear-Right Coxa | Hip rotation |
| CH10 | Rear-Right Femur | Upper leg |
| CH11 | Rear-Right Tibia | Lower leg |
| CH12 | **Scan Servo** | Distance sensor head |
| CH13 | (unused) | Reserved |
| CH14 | (unused) | Reserved |
| CH15 | (unused) | Reserved |

### I2C Address Jumpers
```
    A0  A1  A2  A3  A4  A5    Address
    ─   ─   ─   ─   ─   ─     0x40 (default)
    ▓   ─   ─   ─   ─   ─     0x41
    ─   ▓   ─   ─   ─   ─     0x42
    ▓   ▓   ─   ─   ─   ─     0x43
    ... (up to 62 addresses)

    ─ = Open (not bridged)
    ▓ = Bridged with solder
```

### Power Requirements
| Load | Current (typical) |
|------|-------------------|
| 1 servo idle | ~10mA |
| 1 servo moving | ~200-500mA |
| 12 servos moving | ~2-4A |

> ⚠️ **Use a separate 5-6V power supply rated for at least 4A!**

---

## 4. Complete Pinout Table

### All Used Pins on Milk-V Duo 256M

| Function | GPIO | Physical Pin | Interface | Connected Device |
|----------|------|--------------|-----------|------------------|
| I2C1_SCL | GP4 | J2-4 | I2C1 | PCA9685 SCL |
| I2C1_SDA | GP5 | J2-5 | I2C1 | PCA9685 SDA |
| SPI2_SCLK | GP6 | J2-6 | SPI2 | Eye displays SCL |
| SPI2_MOSI | GP7 | J2-7 | SPI2 | Eye displays SDA |
| SPI2_DC | GP8 (sysfs 506) | J2-8 | GPIO | Eye displays D/C |
| EYE_CS_LEFT | sysfs 505 | J2-25 | GPIO | Left eye CS |
| EYE_CS_RIGHT | sysfs 507 | J2-26 | GPIO | Right eye CS |
| EYE_RST_LEFT | sysfs 451 | J2-31 | GPIO | Left eye RST |
| EYE_RST_RIGHT | sysfs 454 | J2-32 | GPIO | Right eye RST |
| I2C2_SCL | - | - | I2C2 | VL53L0X SCL |
| I2C2_SDA | - | - | I2C2 | VL53L0X SDA |
| 3.3V | - | Pin 1 | Power | All 3.3V devices |
| GND | - | Pin 6 | Power | Common ground |

### Pin Usage Summary
```
     Milk-V Duo 256M - J2 Header (partial)
    ┌─────────────────────────────────────┐
    │  Pin 1 ● 3.3V        GND ● Pin 2    │
    │  Pin 3 ●             ... ●          │
    │  Pin 4 ● GP4 (SCL1)  ... ●          │
    │  Pin 5 ● GP5 (SDA1)  ... ●          │
    │  Pin 6 ● GND         ... ●          │
    │  Pin 7 ● GP6 (SCLK)  ... ●          │
    │  Pin 8 ● GP7 (MOSI)  ... ●          │
    │  Pin 9 ● GP8 (DC)    ... ●          │
    │   ...                               │
    │ Pin 25 ● CS_LEFT    Pin 26 ● CS_R   │
    │   ...                               │
    │ Pin 31 ● RST_LEFT   Pin 32 ● RST_R  │
    └─────────────────────────────────────┘
```

---

## 5. Power Distribution

### Power Rails Overview
```
    ┌───────────────────────────────────────────────────────────┐
    │                      Power Distribution                    │
    └───────────────────────────────────────────────────────────┘

    USB-C Input (5V)                External Power (5-6V, 4A+)
          │                                   │
          ▼                                   ▼
    ┌───────────┐                      ┌───────────┐
    │ Milk-V    │                      │ PCA9685   │
    │ Duo 256M  │                      │ V+ Rail   │
    │           │                      │           │
    │ 3.3V out ─┼──► VL53L0X           │ Servo PWR │
    │           │    PCA9685 Logic     │ CH0-CH12  │
    │           │    Eye displays      │           │
    │   GND  ───┼──────────────────────┼─── GND ◄──┼── Common!
    └───────────┘                      └───────────┘
```

### Recommended Power Supplies
| Rail | Voltage | Current | Source |
|------|---------|---------|--------|
| Logic (Milk-V) | 5V | 500mA | USB-C |
| Servos | 5-6V | 4A+ | External PSU or LiPo BEC |

> ⚠️ **All GND connections MUST be tied together!**

---

## 6. Troubleshooting

### Common Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| I2C device not detected | Wrong address / wiring | Check connections, run `i2cdetect` |
| Servos jitter | Insufficient power | Use beefier power supply |
| Eye display blank | Wrong CS/RST GPIO | Verify sysfs numbers |
| Distance reads 0 | Sensor blocked/disconnected | Check wiring, I2C bus |
| Servo not responding | Wrong channel | Verify channel mapping |

### I2C Bus Scan
```bash
# Scan I2C1 (PCA9685)
i2cdetect -y -r 1

# Expected: 0x40 for PCA9685

# Scan I2C2 (VL53L0X)
i2cdetect -y -r 2

# Expected: 0x29 for VL53L0X
```

### SPI Test
```bash
# Check SPI device exists
ls -la /dev/spidev0.0

# Should show the device file
```

---

## 7. Wiring Checklist

Before powering on, verify:

- [ ] All GND connections are tied together
- [ ] 3.3V only goes to logic pins, NOT servo power
- [ ] External servo power supply is 5-6V, 4A+
- [ ] I2C wires are short (<20cm)
- [ ] No exposed wire ends that could short
- [ ] VL53L0X mounted facing forward
- [ ] Eye displays CS/RST wires go to correct pins
- [ ] PCA9685 address jumpers match code (0x40 default)

---

*Document generated for Spider Robot v3.1 - Milk-V Duo 256M Platform*
