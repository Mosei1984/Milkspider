#!/usr/bin/env python3
"""
Spider Robot v3.1 - Integrated Motion Test
Controls servos to perform basic gaits (stand, sit, wave).
Run directly on Milk-V Duo for hardware-level gait testing.
"""

import time
import math

# PCA9685 Configuration
PCA9685_ADDR = 0x40
MODE1 = 0x00
PRESCALE = 0xFE
LED0_ON_L = 0x06
SERVO_FREQ = 50
SERVO_MIN_US = 500
SERVO_MAX_US = 2500

# Leg servo mapping: leg_name -> {joint: channel}
# Based on PINOUT.md
LEGS = {
    "RF": {"coxa": 0, "femur": 1, "tibia": 2},
    "RM": {"coxa": 3, "femur": 4, "tibia": 5},
    "RH": {"coxa": 6, "femur": 7, "tibia": 8},
    "LF": {"coxa": 9, "femur": 10, "tibia": 11},
}

# Neutral pose (all at 90°)
POSE_NEUTRAL = {leg: {"coxa": 90, "femur": 90, "tibia": 90} for leg in LEGS}

# Stand pose (legs slightly spread, tibia down)
POSE_STAND = {
    "RF": {"coxa": 60, "femur": 60, "tibia": 120},
    "RM": {"coxa": 90, "femur": 60, "tibia": 120},
    "RH": {"coxa": 120, "femur": 60, "tibia": 120},
    "LF": {"coxa": 120, "femur": 60, "tibia": 120},
}

# Sit pose (legs folded)
POSE_SIT = {
    "RF": {"coxa": 60, "femur": 120, "tibia": 60},
    "RM": {"coxa": 90, "femur": 120, "tibia": 60},
    "RH": {"coxa": 120, "femur": 120, "tibia": 60},
    "LF": {"coxa": 120, "femur": 120, "tibia": 60},
}

class PCA9685:
    def __init__(self, bus_num=1, addr=PCA9685_ADDR):
        self.addr = addr
        try:
            import smbus2 as smbus
        except ImportError:
            import smbus
        self.bus = smbus.SMBus(bus_num)
        self._init()

    def _init(self):
        self.bus.write_byte_data(self.addr, MODE1, 0x80)
        time.sleep(0.01)
        prescale = int(25000000.0 / (4096 * SERVO_FREQ) - 1)
        old_mode = self.bus.read_byte_data(self.addr, MODE1)
        self.bus.write_byte_data(self.addr, MODE1, (old_mode & 0x7F) | 0x10)
        self.bus.write_byte_data(self.addr, PRESCALE, prescale)
        self.bus.write_byte_data(self.addr, MODE1, old_mode)
        time.sleep(0.005)
        self.bus.write_byte_data(self.addr, MODE1, old_mode | 0xA0)

    def set_pwm(self, ch, on, off):
        reg = LED0_ON_L + 4 * ch
        self.bus.write_i2c_block_data(self.addr, reg, [on & 0xFF, on >> 8, off & 0xFF, off >> 8])

    def set_angle(self, ch, angle):
        angle = max(0, min(180, angle))
        pulse_us = SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * angle / 180
        off_val = int(pulse_us * 4096 / 20000)
        self.set_pwm(ch, 0, off_val)

    def disable_all(self):
        for ch in range(16):
            self.set_pwm(ch, 0, 0)


class SpiderMotion:
    def __init__(self):
        self.pca = PCA9685()
        self.current_pose = {leg: dict(joints) for leg, joints in POSE_NEUTRAL.items()}
        print("SpiderMotion initialized")

    def set_pose(self, pose, duration_s=0.5):
        """Smoothly transition to a target pose."""
        steps = int(duration_s * 50)
        if steps < 1:
            steps = 1

        for step in range(steps + 1):
            t = step / steps
            for leg_name, joints in pose.items():
                if leg_name not in LEGS:
                    continue
                for joint_name, target_angle in joints.items():
                    if joint_name not in LEGS[leg_name]:
                        continue
                    ch = LEGS[leg_name][joint_name]
                    start = self.current_pose.get(leg_name, {}).get(joint_name, 90)
                    angle = int(start + (target_angle - start) * t)
                    self.pca.set_angle(ch, angle)
            time.sleep(0.02)

        self.current_pose = {leg: dict(joints) for leg, joints in pose.items()}

    def stand(self):
        print("Standing...")
        self.set_pose(POSE_STAND, 1.0)

    def sit(self):
        print("Sitting...")
        self.set_pose(POSE_SIT, 1.0)

    def neutral(self):
        print("Neutral position...")
        self.set_pose(POSE_NEUTRAL, 1.0)

    def wave(self, leg="RF"):
        """Wave with front right leg."""
        print(f"Waving with {leg}...")
        if leg not in LEGS:
            return

        # Lift leg
        wave_up = dict(self.current_pose)
        wave_up[leg] = {"coxa": 45, "femur": 30, "tibia": 90}
        self.set_pose(wave_up, 0.5)

        # Wave motion
        for _ in range(3):
            wave_left = dict(wave_up)
            wave_left[leg] = {"coxa": 30, "femur": 30, "tibia": 90}
            self.set_pose(wave_left, 0.2)

            wave_right = dict(wave_up)
            wave_right[leg] = {"coxa": 60, "femur": 30, "tibia": 90}
            self.set_pose(wave_right, 0.2)

        # Return to stand
        self.stand()

    def walk_step(self):
        """Single tripod gait step (RF, LM, RH lift together)."""
        print("Walk step...")

        # Tripod 1: RF, RM move forward (lift and swing)
        lift1 = {
            "RF": {"coxa": 45, "femur": 45, "tibia": 120},
            "RM": {"coxa": 75, "femur": 45, "tibia": 120},
            "RH": {"coxa": 120, "femur": 60, "tibia": 120},
            "LF": {"coxa": 120, "femur": 60, "tibia": 120},
        }
        self.set_pose(lift1, 0.3)

        # Land tripod 1
        land1 = {
            "RF": {"coxa": 75, "femur": 60, "tibia": 120},
            "RM": {"coxa": 105, "femur": 60, "tibia": 120},
            "RH": {"coxa": 120, "femur": 60, "tibia": 120},
            "LF": {"coxa": 120, "femur": 60, "tibia": 120},
        }
        self.set_pose(land1, 0.3)

        # Tripod 2: RH, LF move forward
        lift2 = {
            "RF": {"coxa": 75, "femur": 60, "tibia": 120},
            "RM": {"coxa": 105, "femur": 60, "tibia": 120},
            "RH": {"coxa": 105, "femur": 45, "tibia": 120},
            "LF": {"coxa": 105, "femur": 45, "tibia": 120},
        }
        self.set_pose(lift2, 0.3)

        # Land tripod 2 and reset body
        self.stand()

    def shutdown(self):
        print("Shutting down...")
        self.neutral()
        time.sleep(0.3)
        self.pca.disable_all()


def interactive():
    """Interactive motion testing."""
    spider = SpiderMotion()

    print("\n=== Spider Motion Test ===")
    print("Commands: stand, sit, neutral, wave, walk, quit")

    try:
        while True:
            cmd = input("> ").strip().lower()
            if cmd == "quit" or cmd == "q":
                break
            elif cmd == "stand":
                spider.stand()
            elif cmd == "sit":
                spider.sit()
            elif cmd == "neutral":
                spider.neutral()
            elif cmd == "wave":
                spider.wave()
            elif cmd == "walk":
                spider.walk_step()
            else:
                print("Unknown command")
    except KeyboardInterrupt:
        pass

    spider.shutdown()


def demo():
    """Run a short demo sequence."""
    spider = SpiderMotion()

    print("\n=== Spider Demo ===")
    time.sleep(1)

    spider.stand()
    time.sleep(1)

    spider.wave()
    time.sleep(1)

    spider.walk_step()
    time.sleep(0.5)
    spider.walk_step()
    time.sleep(1)

    spider.sit()
    time.sleep(1)

    spider.shutdown()
    print("Demo complete")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "--demo":
        demo()
    else:
        interactive()
