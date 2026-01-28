#!/usr/bin/env python3
"""
Spider Robot v3.1 - Servo Calibration Tool

Interactive calibration system for per-channel servo offset adjustment.
Offsets are stored in ~/.spider_calibration.json and applied to PWM values
to ensure mechanical center matches 90°.

Calibration range: -30° to +30° (from limits.h CALIB_OFFSET_MIN/MAX_DEG)
"""

import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Any

try:
    from spider_client import SpiderClient
except ImportError:
    from .spider_client import SpiderClient

CALIB_OFFSET_MIN_DEG = -30
CALIB_OFFSET_MAX_DEG = 30
SERVO_COUNT = 12
SERVO_COUNT_TOTAL = 13
SERVO_NEUTRAL_US = 1500

DEFAULT_CALIB_PATH = Path.home() / ".spider_calibration.json"

CHANNEL_NOTES = {
    "0": "FR coxa",
    "1": "FR femur",
    "2": "FR tibia",
    "3": "FL coxa",
    "4": "FL femur",
    "5": "FL tibia",
    "6": "RR coxa",
    "7": "RR femur",
    "8": "RR tibia",
    "9": "RL coxa",
    "10": "RL femur",
    "11": "RL tibia",
    "12": "Scan servo",
}

LEG_INFO = {
    0: {"name": "Front Right (FR)", "channels": [0, 1, 2]},
    1: {"name": "Front Left (FL)", "channels": [3, 4, 5]},
    2: {"name": "Rear Right (RR)", "channels": [6, 7, 8]},
    3: {"name": "Rear Left (RL)", "channels": [9, 10, 11]},
}

JOINT_NAMES = ["coxa", "femur", "tibia"]


def deg_to_us_offset(deg: float) -> int:
    """Convert degree offset to microseconds offset (11.11 us/deg)."""
    return int(deg * (2000.0 / 180.0))


def us_to_deg_offset(us: int) -> float:
    """Convert microseconds offset to degrees."""
    return us * (180.0 / 2000.0)


class CalibrationData:
    """Manages calibration data loading/saving."""
    
    def __init__(self, path: Optional[Path] = None):
        self.path = path or DEFAULT_CALIB_PATH
        self.version = 1
        self.offsets_deg: List[float] = [0.0] * SERVO_COUNT_TOTAL
        self.notes: Dict[str, str] = CHANNEL_NOTES.copy()
    
    def load(self) -> bool:
        """Load calibration from JSON file. Returns True if loaded successfully."""
        if not self.path.exists():
            print(f"No calibration file found at {self.path}")
            return False
        
        try:
            with open(self.path, 'r') as f:
                data = json.load(f)
            
            self.version = data.get("version", 1)
            offsets = data.get("offsets_deg", [])
            
            if len(offsets) < SERVO_COUNT_TOTAL:
                offsets.extend([0.0] * (SERVO_COUNT_TOTAL - len(offsets)))
            
            self.offsets_deg = [
                max(CALIB_OFFSET_MIN_DEG, min(CALIB_OFFSET_MAX_DEG, float(o)))
                for o in offsets[:SERVO_COUNT_TOTAL]
            ]
            
            self.notes = data.get("notes", CHANNEL_NOTES.copy())
            print(f"Loaded calibration from {self.path}")
            return True
            
        except (json.JSONDecodeError, IOError) as e:
            print(f"Error loading calibration: {e}")
            return False
    
    def save(self) -> bool:
        """Save calibration to JSON file."""
        try:
            data = {
                "version": self.version,
                "offsets_deg": self.offsets_deg,
                "notes": self.notes,
            }
            
            with open(self.path, 'w') as f:
                json.dump(data, f, indent=2)
            
            print(f"Saved calibration to {self.path}")
            return True
            
        except IOError as e:
            print(f"Error saving calibration: {e}")
            return False
    
    def get_offset_deg(self, channel: int) -> float:
        """Get offset for a channel in degrees."""
        if 0 <= channel < len(self.offsets_deg):
            return self.offsets_deg[channel]
        return 0.0
    
    def set_offset_deg(self, channel: int, offset_deg: float) -> bool:
        """Set offset for a channel in degrees. Returns True if valid."""
        if not 0 <= channel < SERVO_COUNT_TOTAL:
            return False
        
        offset_deg = max(CALIB_OFFSET_MIN_DEG, min(CALIB_OFFSET_MAX_DEG, offset_deg))
        self.offsets_deg[channel] = offset_deg
        return True
    
    def get_offset_us(self, channel: int) -> int:
        """Get offset for a channel in microseconds."""
        return deg_to_us_offset(self.get_offset_deg(channel))
    
    def get_all_offsets_us(self) -> List[int]:
        """Get all offsets as microseconds."""
        return [deg_to_us_offset(o) for o in self.offsets_deg]
    
    def apply_to_us(self, channel: int, us: int) -> int:
        """Apply calibration offset to a PWM value."""
        offset_us = self.get_offset_us(channel)
        calibrated = us + offset_us
        return max(500, min(2500, calibrated))
    
    def print_summary(self):
        """Print current calibration values."""
        print("\n=== Current Calibration ===")
        print(f"File: {self.path}")
        print(f"{'CH':<4} {'Offset(°)':<10} {'Offset(µs)':<12} {'Note'}")
        print("-" * 50)
        for i, offset in enumerate(self.offsets_deg):
            us = deg_to_us_offset(offset)
            note = self.notes.get(str(i), "")
            print(f"{i:<4} {offset:>+8.1f}°  {us:>+10}µs   {note}")


class CalibrationTool:
    """Interactive calibration tool for Spider Robot."""
    
    def __init__(self, client: SpiderClient, calib_path: Optional[Path] = None):
        self.client = client
        self.calib = CalibrationData(calib_path)
        self.calib.load()
    
    def move_servo_calibrated(self, channel: int, angle_deg: float = 90.0):
        """Move a servo to an angle, applying calibration offset."""
        base_us = int(500 + (angle_deg / 180.0) * 2000)
        calibrated_us = self.calib.apply_to_us(channel, base_us)
        
        if channel == 12:
            self.client.scan_set(calibrated_us)
        else:
            self.client.set_servo(channel, calibrated_us)
    
    def move_servo_raw(self, channel: int, us: int):
        """Move a servo to a raw PWM value (no calibration)."""
        if channel == 12:
            self.client.scan_set(us)
        else:
            self.client.set_servo(channel, us)
    
    def calibrate_channel(self, channel: int) -> Optional[float]:
        """
        Interactively calibrate a single channel.
        Returns the final offset in degrees, or None if cancelled.
        """
        note = self.calib.notes.get(str(channel), f"Channel {channel}")
        current_offset = self.calib.get_offset_deg(channel)
        
        print(f"\n=== Calibrating CH{channel}: {note} ===")
        print(f"Current offset: {current_offset:+.1f}°")
        print("\nCommands:")
        print("  +/-N   : Adjust offset by N degrees (e.g., +5, -2.5)")
        print("  =N     : Set offset to N degrees (e.g., =0, =-10)")
        print("  c      : Center servo (send 90° + current offset)")
        print("  t      : Test sweep (move 60° -> 90° -> 120°)")
        print("  r      : Reset offset to 0")
        print("  s      : Save and continue to next channel")
        print("  q      : Save and quit calibration")
        print("  x      : Discard changes and quit")
        
        temp_offset = current_offset
        
        self.move_servo_raw(channel, SERVO_NEUTRAL_US + deg_to_us_offset(temp_offset))
        
        while True:
            try:
                cmd = input(f"\n[CH{channel} offset={temp_offset:+.1f}°] > ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                print("\nCancelled")
                return None
            
            if not cmd:
                continue
            
            if cmd == 'c':
                us = SERVO_NEUTRAL_US + deg_to_us_offset(temp_offset)
                self.move_servo_raw(channel, us)
                print(f"Centered at {us}µs (90° + {temp_offset:+.1f}° offset)")
            
            elif cmd == 't':
                print("Test sweep: 60° -> 90° -> 120°")
                import time
                for angle in [60, 90, 120, 90]:
                    base_us = int(500 + (angle / 180.0) * 2000)
                    us = base_us + deg_to_us_offset(temp_offset)
                    self.move_servo_raw(channel, us)
                    print(f"  {angle}° -> {us}µs")
                    time.sleep(0.5)
            
            elif cmd == 'r':
                temp_offset = 0.0
                self.move_servo_raw(channel, SERVO_NEUTRAL_US)
                print("Offset reset to 0°")
            
            elif cmd == 's':
                self.calib.set_offset_deg(channel, temp_offset)
                print(f"Saved CH{channel} offset: {temp_offset:+.1f}°")
                return temp_offset
            
            elif cmd == 'q':
                self.calib.set_offset_deg(channel, temp_offset)
                self.calib.save()
                print("Saved and exiting calibration mode")
                return None
            
            elif cmd == 'x':
                print("Discarding changes")
                return None
            
            elif cmd.startswith('+') or cmd.startswith('-'):
                try:
                    delta = float(cmd)
                    temp_offset = max(CALIB_OFFSET_MIN_DEG, 
                                     min(CALIB_OFFSET_MAX_DEG, temp_offset + delta))
                    us = SERVO_NEUTRAL_US + deg_to_us_offset(temp_offset)
                    self.move_servo_raw(channel, us)
                    print(f"Offset: {temp_offset:+.1f}° -> {us}µs")
                except ValueError:
                    print("Invalid number")
            
            elif cmd.startswith('='):
                try:
                    temp_offset = float(cmd[1:])
                    temp_offset = max(CALIB_OFFSET_MIN_DEG, 
                                     min(CALIB_OFFSET_MAX_DEG, temp_offset))
                    us = SERVO_NEUTRAL_US + deg_to_us_offset(temp_offset)
                    self.move_servo_raw(channel, us)
                    print(f"Offset: {temp_offset:+.1f}° -> {us}µs")
                except ValueError:
                    print("Invalid number")
            
            else:
                print("Unknown command. Use +N, -N, =N, c, t, r, s, q, or x")
    
    def calibrate_leg(self, leg_index: int) -> bool:
        """
        Calibrate all three servos of a leg.
        Returns True if completed, False if cancelled.
        """
        if leg_index not in LEG_INFO:
            print(f"Invalid leg index: {leg_index}")
            return False
        
        leg = LEG_INFO[leg_index]
        print(f"\n{'='*50}")
        print(f"=== Calibrating Leg {leg_index}: {leg['name']} ===")
        print(f"{'='*50}")
        
        for i, channel in enumerate(leg['channels']):
            joint = JOINT_NAMES[i]
            print(f"\n--- Joint: {joint} (CH{channel}) ---")
            result = self.calibrate_channel(channel)
            if result is None:
                return False
        
        return True
    
    def calibrate_all_legs(self):
        """Run leg-by-leg calibration workflow."""
        print("\n" + "="*60)
        print("  SPIDER ROBOT v3.1 - LEG CALIBRATION WORKFLOW")
        print("="*60)
        print("\nThis will guide you through calibrating all 4 legs.")
        print("For each servo, adjust the offset until mechanical center = 90°")
        print(f"\nOffset range: {CALIB_OFFSET_MIN_DEG}° to {CALIB_OFFSET_MAX_DEG}°")
        
        self.calib.print_summary()
        
        input("\nPress Enter to start, or Ctrl+C to cancel...")
        
        for leg_index in range(4):
            if not self.calibrate_leg(leg_index):
                print("\nCalibration cancelled")
                break
        
        self.calib.save()
        self.calib.print_summary()
    
    def calibrate_scan_servo(self):
        """Calibrate the scan servo (CH12)."""
        print("\n=== Calibrating Scan Servo (CH12) ===")
        result = self.calibrate_channel(12)
        if result is not None:
            self.calib.save()
    
    def run_interactive(self):
        """Run main interactive calibration menu."""
        print("\n" + "="*60)
        print("  SPIDER ROBOT v3.1 - SERVO CALIBRATION TOOL")
        print("="*60)
        
        while True:
            print("\n--- Main Menu ---")
            print("1. Calibrate all legs (leg-by-leg workflow)")
            print("2. Calibrate single channel")
            print("3. Calibrate scan servo (CH12)")
            print("4. Show current calibration")
            print("5. Reset all calibration to zero")
            print("6. Save and exit")
            print("q. Quit without saving")
            
            try:
                choice = input("\nChoice: ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                print("\nExiting")
                break
            
            if choice == '1':
                self.calibrate_all_legs()
            
            elif choice == '2':
                try:
                    ch = int(input("Enter channel (0-11): "))
                    if 0 <= ch < SERVO_COUNT:
                        result = self.calibrate_channel(ch)
                        if result is not None:
                            self.calib.save()
                    else:
                        print("Invalid channel")
                except ValueError:
                    print("Invalid input")
            
            elif choice == '3':
                self.calibrate_scan_servo()
            
            elif choice == '4':
                self.calib.print_summary()
            
            elif choice == '5':
                confirm = input("Reset ALL offsets to 0? (yes/no): ").strip().lower()
                if confirm == 'yes':
                    self.calib.offsets_deg = [0.0] * SERVO_COUNT_TOTAL
                    self.calib.save()
                    print("All offsets reset to 0")
            
            elif choice == '6':
                self.calib.save()
                print("Saved and exiting")
                break
            
            elif choice == 'q':
                print("Exiting without saving")
                break


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Spider Robot v3.1 Servo Calibration Tool")
    parser.add_argument("--ip", default="192.168.42.1", help="Brain daemon IP address")
    parser.add_argument("--port", type=int, default=9000, help="Brain daemon port")
    parser.add_argument("--calib-file", type=str, default=None, 
                       help=f"Calibration file path (default: {DEFAULT_CALIB_PATH})")
    parser.add_argument("--show", action="store_true", help="Show current calibration and exit")
    parser.add_argument("--leg", type=int, choices=[0, 1, 2, 3], 
                       help="Calibrate specific leg only")
    parser.add_argument("--channel", type=int, help="Calibrate specific channel only")
    args = parser.parse_args()
    
    calib_path = Path(args.calib_file) if args.calib_file else None
    
    if args.show:
        calib = CalibrationData(calib_path)
        calib.load()
        calib.print_summary()
        return 0
    
    client = SpiderClient(host=args.ip, port=args.port)
    
    if not client.connect():
        print("Failed to connect to Brain daemon")
        return 1
    
    try:
        tool = CalibrationTool(client, calib_path)
        
        if args.channel is not None:
            if 0 <= args.channel < SERVO_COUNT_TOTAL:
                result = tool.calibrate_channel(args.channel)
                if result is not None:
                    tool.calib.save()
            else:
                print(f"Invalid channel: {args.channel}")
        elif args.leg is not None:
            tool.calibrate_leg(args.leg)
            tool.calib.save()
        else:
            tool.run_interactive()
    
    finally:
        client.all_neutral()
        client.disconnect()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
