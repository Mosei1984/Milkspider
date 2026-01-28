#!/usr/bin/env python3
"""
Spider Robot v3.1 - Calibrated Client Wrapper

A wrapper around SpiderClient that automatically loads and applies
calibration offsets to all servo commands.

Usage:
    from calibrated_client import CalibratedSpiderClient
    
    with CalibratedSpiderClient() as client:
        client.set_servo(0, 1500)  # Calibration applied automatically
        client.all_neutral()       # All servos go to calibrated neutral
"""

from pathlib import Path
from typing import Optional, List, Dict, Any

try:
    from spider_client import SpiderClient
    from calibration import CalibrationData, DEFAULT_CALIB_PATH, deg_to_us_offset
except ImportError:
    from .spider_client import SpiderClient
    from .calibration import CalibrationData, DEFAULT_CALIB_PATH, deg_to_us_offset


class CalibratedSpiderClient(SpiderClient):
    """
    SpiderClient wrapper that applies calibration offsets to all servo commands.
    
    Loads calibration from ~/.spider_calibration.json on init.
    All servo PWM values are adjusted by per-channel offsets before sending.
    """
    
    def __init__(self, host: str = "192.168.42.1", port: int = 9000,
                 calib_path: Optional[Path] = None, 
                 auto_load_calib: bool = True):
        """
        Initialize calibrated client.
        
        Args:
            host: Brain daemon IP address
            port: Brain daemon port
            calib_path: Path to calibration JSON file (default: ~/.spider_calibration.json)
            auto_load_calib: If True, load calibration file on init
        """
        super().__init__(host, port)
        
        self.calib = CalibrationData(calib_path or DEFAULT_CALIB_PATH)
        self._calibration_enabled = True
        
        if auto_load_calib:
            self.calib.load()
    
    @property
    def calibration_enabled(self) -> bool:
        """Check if calibration is currently enabled."""
        return self._calibration_enabled
    
    @calibration_enabled.setter
    def calibration_enabled(self, enabled: bool):
        """Enable or disable calibration."""
        self._calibration_enabled = enabled
    
    def reload_calibration(self) -> bool:
        """Reload calibration from file."""
        return self.calib.load()
    
    def get_calibration_offset(self, channel: int) -> float:
        """Get calibration offset for a channel in degrees."""
        return self.calib.get_offset_deg(channel)
    
    def get_calibration_offsets(self) -> List[float]:
        """Get all calibration offsets in degrees."""
        return self.calib.offsets_deg.copy()
    
    def _apply_calibration(self, channel: int, us: int) -> int:
        """Apply calibration offset to a PWM value if enabled."""
        if not self._calibration_enabled:
            return us
        return self.calib.apply_to_us(channel, us)
    
    def set_servo(self, channel: int, us: int) -> bool:
        """Set a single servo position with calibration applied."""
        if not 0 <= channel < self.SERVO_COUNT:
            print(f"ERROR: Invalid channel {channel}. Must be 0-{self.SERVO_COUNT-1}")
            return False
        
        calibrated_us = self._apply_calibration(channel, us)
        calibrated_us = max(self.SERVO_MIN_US, min(self.SERVO_MAX_US, calibrated_us))
        
        command = {"type": "servo", "channel": channel, "us": calibrated_us}
        self.send_command(command)
        return True
    
    def set_servo_raw(self, channel: int, us: int) -> bool:
        """Set a single servo position WITHOUT calibration (bypass)."""
        return super().set_servo(channel, us)
    
    def set_all_servos(self, us_values: List[int]) -> bool:
        """Set all servo positions with calibration applied."""
        if len(us_values) != self.SERVO_COUNT:
            print(f"ERROR: Expected {self.SERVO_COUNT} values, got {len(us_values)}")
            return False
        
        calibrated_values = []
        for channel, us in enumerate(us_values):
            calibrated_us = self._apply_calibration(channel, us)
            calibrated_us = max(self.SERVO_MIN_US, min(self.SERVO_MAX_US, calibrated_us))
            calibrated_values.append(calibrated_us)
        
        command = {"type": "servos", "us": calibrated_values}
        self.send_command(command)
        return True
    
    def set_all_servos_raw(self, us_values: List[int]) -> bool:
        """Set all servo positions WITHOUT calibration (bypass)."""
        return super().set_all_servos(us_values)
    
    def set_leg(self, leg: int, coxa_us: int, femur_us: int, tibia_us: int) -> bool:
        """Set all three servos of a leg with calibration applied."""
        if leg not in self.LEG_CHANNELS:
            print(f"ERROR: Invalid leg {leg}. Must be 0-3")
            return False
        
        channels = self.LEG_CHANNELS[leg]
        self.set_servo(channels[0], coxa_us)
        self.set_servo(channels[1], femur_us)
        self.set_servo(channels[2], tibia_us)
        return True
    
    def all_neutral(self) -> bool:
        """
        Set all servos to neutral position with calibration applied.
        
        This means each servo goes to its calibrated 90° position,
        which should be mechanical center after calibration.
        """
        print("Setting all servos to calibrated neutral (1500us + offsets)")
        return self.set_all_servos([self.SERVO_NEUTRAL_US] * self.SERVO_COUNT)
    
    def all_neutral_raw(self) -> bool:
        """Set all servos to raw 1500us (no calibration)."""
        print("Setting all servos to raw neutral (1500us)")
        return super().all_neutral()
    
    def scan_set(self, us: int) -> bool:
        """Set scan servo position with calibration applied."""
        calibrated_us = self._apply_calibration(self.SERVO_CHANNEL_SCAN, us)
        calibrated_us = max(self.SERVO_MIN_US, min(self.SERVO_MAX_US, calibrated_us))
        command = {"type": "scan", "us": calibrated_us}
        self.send_command(command)
        return True
    
    def scan_set_raw(self, us: int) -> bool:
        """Set scan servo position WITHOUT calibration (bypass)."""
        return super().scan_set(us)
    
    def print_calibration_summary(self):
        """Print current calibration values."""
        self.calib.print_summary()


def main():
    """Test calibrated client."""
    import argparse
    import json
    
    parser = argparse.ArgumentParser(description="Calibrated Spider Client - Test")
    parser.add_argument("--ip", default="192.168.42.1", help="Brain daemon IP address")
    parser.add_argument("--port", type=int, default=9000, help="Brain daemon port")
    parser.add_argument("--show-calib", action="store_true", help="Show calibration and exit")
    args = parser.parse_args()
    
    client = CalibratedSpiderClient(host=args.ip, port=args.port)
    
    if args.show_calib:
        client.print_calibration_summary()
        return 0
    
    if client.connect():
        print("\n--- Calibration Summary ---")
        client.print_calibration_summary()
        
        print("\n--- Testing status query ---")
        status = client.get_status()
        if status:
            print(f"Status: {json.dumps(status, indent=2)}")
        
        print("\n--- Setting calibrated neutral ---")
        client.all_neutral()
        
        client.disconnect()
    else:
        print("Failed to connect to Brain daemon")
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())
