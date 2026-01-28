#!/usr/bin/env python3
"""
Spider Robot v3.1 - Hardware Test Script
Tests I2C and SPI connectivity on Milk-V Duo 256M
"""

import os
import sys
import time

print("=== Spider v3.1 Hardware Test ===")
print(f"Python: {sys.version}")
print()

# Test I2C
print("[I2C Test]")
try:
    # Try smbus2 first, then smbus
    try:
        import smbus2 as smbus
        print("  Using smbus2")
    except ImportError:
        import smbus
        print("  Using smbus")
    
    # Test I2C1 (PCA9685)
    try:
        bus1 = smbus.SMBus(1)
        print("  I2C-1: OK")
        
        # Try to detect PCA9685 at 0x40
        try:
            bus1.read_byte(0x40)
            print("  PCA9685 (0x40): FOUND")
        except:
            print("  PCA9685 (0x40): Not connected")
        bus1.close()
    except Exception as e:
        print(f"  I2C-1: ERROR - {e}")
    
    # Test I2C2 (VL53L0X)
    try:
        bus2 = smbus.SMBus(2)
        print("  I2C-2: OK")
        
        # Try to detect VL53L0X at 0x29
        try:
            bus2.read_byte(0x29)
            print("  VL53L0X (0x29): FOUND")
        except:
            print("  VL53L0X (0x29): Not connected")
        bus2.close()
    except Exception as e:
        print(f"  I2C-2: ERROR - {e}")

except ImportError:
    print("  smbus/smbus2 not available")
    print("  Testing via /dev/i2c-* directly...")
    
    for bus_num in [1, 2]:
        path = f"/dev/i2c-{bus_num}"
        if os.path.exists(path):
            print(f"  {path}: EXISTS")
        else:
            print(f"  {path}: NOT FOUND")

print()

# Test SPI
print("[SPI Test]")
try:
    import spidev
    print("  spidev module: OK")
    
    try:
        spi = spidev.SpiDev()
        spi.open(0, 0)  # spidev0.0
        spi.max_speed_hz = 1000000
        print("  /dev/spidev0.0: OK")
        
        # Send test byte
        resp = spi.xfer2([0x00])
        print(f"  SPI TX/RX test: {resp}")
        spi.close()
    except Exception as e:
        print(f"  SPI open error: {e}")
        
except ImportError:
    print("  spidev not available")
    for spi_dev in ["/dev/spidev0.0", "/dev/spidev1.0"]:
        if os.path.exists(spi_dev):
            print(f"  {spi_dev}: EXISTS")

print()

# Test GPIO
print("[GPIO Test]")
gpio_chips = [352, 384, 416, 448, 480]
for chip in gpio_chips:
    path = f"/sys/class/gpio/gpiochip{chip}"
    if os.path.exists(path):
        # Read label
        try:
            with open(f"{path}/label", "r") as f:
                label = f.read().strip()
            print(f"  gpiochip{chip}: {label}")
        except:
            print(f"  gpiochip{chip}: OK")

print()
print("=== Test Complete ===")
