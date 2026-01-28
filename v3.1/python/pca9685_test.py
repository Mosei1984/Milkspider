#!/usr/bin/env python3
"""
Spider Robot v3.1 - PCA9685 Servo Controller Test
Directly controls servos via I2C to verify hardware before C++ deployment.
"""

import time
import struct

# PCA9685 registers
PCA9685_ADDR = 0x40
MODE1 = 0x00
MODE2 = 0x01
PRESCALE = 0xFE
LED0_ON_L = 0x06
LED0_ON_H = 0x07
LED0_OFF_L = 0x08
LED0_OFF_H = 0x09

# Servo configuration
SERVO_FREQ = 50  # Hz (standard for servos)
SERVO_MIN_US = 500   # microseconds
SERVO_MAX_US = 2500  # microseconds

class PCA9685:
    def __init__(self, bus_num=1, addr=PCA9685_ADDR):
        self.addr = addr
        try:
            import smbus2 as smbus
        except ImportError:
            import smbus
        self.bus = smbus.SMBus(bus_num)
        self._init_pca9685()

    def _init_pca9685(self):
        """Initialize PCA9685 for servo control at 50Hz."""
        # Reset
        self.bus.write_byte_data(self.addr, MODE1, 0x80)
        time.sleep(0.01)
        
        # Set frequency
        prescale = int(25000000.0 / (4096 * SERVO_FREQ) - 1)
        
        # Sleep mode to set prescaler
        old_mode = self.bus.read_byte_data(self.addr, MODE1)
        self.bus.write_byte_data(self.addr, MODE1, (old_mode & 0x7F) | 0x10)
        self.bus.write_byte_data(self.addr, PRESCALE, prescale)
        self.bus.write_byte_data(self.addr, MODE1, old_mode)
        time.sleep(0.005)
        
        # Auto-increment enabled
        self.bus.write_byte_data(self.addr, MODE1, old_mode | 0xA0)
        print(f"PCA9685 initialized: prescale={prescale}, freq~{SERVO_FREQ}Hz")

    def set_pwm(self, channel, on, off):
        """Set PWM for a channel."""
        reg = LED0_ON_L + 4 * channel
        self.bus.write_i2c_block_data(self.addr, reg, [on & 0xFF, on >> 8, off & 0xFF, off >> 8])

    def set_servo_us(self, channel, pulse_us):
        """Set servo position in microseconds (500-2500)."""
        pulse_us = max(SERVO_MIN_US, min(SERVO_MAX_US, pulse_us))
        # Convert to 12-bit value (0-4095 over 20ms period)
        off_val = int(pulse_us * 4096 / 20000)
        self.set_pwm(channel, 0, off_val)

    def set_servo_angle(self, channel, angle):
        """Set servo angle (0-180 degrees)."""
        angle = max(0, min(180, angle))
        pulse_us = SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * angle / 180
        self.set_servo_us(channel, int(pulse_us))

    def disable_all(self):
        """Disable all PWM outputs."""
        for ch in range(16):
            self.set_pwm(ch, 0, 0)


def test_servos():
    """Test servo movement on all 12 leg servos."""
    print("=== PCA9685 Servo Test ===\n")
    
    try:
        pca = PCA9685(bus_num=1)
    except Exception as e:
        print(f"ERROR: Cannot open PCA9685: {e}")
        return False

    # Spider leg servo mapping (from PINOUT.md)
    legs = {
        "RF": {"coxa": 0, "femur": 1, "tibia": 2},
        "RM": {"coxa": 3, "femur": 4, "tibia": 5},
        "RH": {"coxa": 6, "femur": 7, "tibia": 8},
        "LF": {"coxa": 9, "femur": 10, "tibia": 11},
        "LM": {"coxa": 12, "femur": 13, "tibia": 14},
        "LH": {"coxa": 15, "femur": 16, "tibia": 17},  # ch 16-17 on second PCA9685 if present
    }

    print("Testing first 12 channels (single PCA9685)...")
    
    # Center all servos
    print("\n[1] Centering all servos (90°)...")
    for ch in range(12):
        pca.set_servo_angle(ch, 90)
    time.sleep(1)

    # Sweep test
    print("[2] Sweep test (45° -> 135° -> 90°)...")
    for angle in [45, 135, 90]:
        print(f"    All servos to {angle}°")
        for ch in range(12):
            pca.set_servo_angle(ch, angle)
        time.sleep(0.8)

    # Individual leg test
    print("\n[3] Individual leg test...")
    for leg_name, joints in list(legs.items())[:4]:  # First 4 legs
        print(f"    Testing {leg_name} leg...")
        for joint_name, ch in joints.items():
            if ch < 12:
                pca.set_servo_angle(ch, 60)
                time.sleep(0.15)
                pca.set_servo_angle(ch, 120)
                time.sleep(0.15)
                pca.set_servo_angle(ch, 90)
                time.sleep(0.1)

    print("\n[4] Disabling outputs...")
    pca.disable_all()
    
    print("\n=== Servo Test Complete ===")
    return True


def test_single_servo(channel=0):
    """Interactive single servo test."""
    print(f"=== Single Servo Test (Channel {channel}) ===\n")
    
    try:
        pca = PCA9685(bus_num=1)
    except Exception as e:
        print(f"ERROR: {e}")
        return

    print("Enter angle (0-180), 'c' to center, 'q' to quit:")
    while True:
        try:
            cmd = input("> ").strip().lower()
            if cmd == 'q':
                break
            elif cmd == 'c':
                pca.set_servo_angle(channel, 90)
                print("Centered (90°)")
            else:
                angle = int(cmd)
                pca.set_servo_angle(channel, angle)
                print(f"Set to {angle}°")
        except ValueError:
            print("Invalid input")
        except KeyboardInterrupt:
            break

    pca.disable_all()
    print("Done")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "--single":
        ch = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        test_single_servo(ch)
    else:
        test_servos()
