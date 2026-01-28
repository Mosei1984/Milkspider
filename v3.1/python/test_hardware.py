#!/usr/bin/env python3
"""
Spider Robot v3.1 - Master Hardware Test Suite

Comprehensive test script for all hardware components:
- Connection/WebSocket communication
- Leg servos (CH0-11)
- Scan servo (CH12)
- Eyes (moods, look directions, blink/wink)
- Distance sensor (VL53L0X)
- Coordinated system tests

Usage:
    python test_hardware.py                    # Full system test
    python test_hardware.py --test-servos      # Servo tests only
    python test_hardware.py --test-eyes        # Eye tests only
    python test_hardware.py --test-distance    # Distance sensor only
    python test_hardware.py --simulate         # Mock hardware responses
"""

import argparse
import json
import sys
import time
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any


@dataclass
class TestResult:
    """Result of a single test."""
    name: str
    passed: bool
    message: str = ""
    details: Dict[str, Any] = field(default_factory=dict)


class MockSpiderClient:
    """Mock client for simulation mode."""
    
    SERVO_COUNT = 12
    SERVO_NEUTRAL_US = 1500
    LEG_NAMES = {0: "FR", 1: "FL", 2: "RR", 3: "RL"}
    JOINT_NAMES = {0: "coxa", 1: "femur", 2: "tibia"}
    LEG_CHANNELS = {
        0: (0, 1, 2),
        1: (3, 4, 5),
        2: (6, 7, 8),
        3: (9, 10, 11),
    }
    
    def __init__(self, host: str = "192.168.42.1", port: int = 9000):
        self.host = host
        self.port = port
        self.connected = False
        self._mock_distance = 500
        self._distance_variance = 50
    
    def connect(self) -> bool:
        print(f"[SIMULATE] Connecting to ws://{self.host}:{self.port}...")
        time.sleep(0.1)
        self.connected = True
        print(f"[SIMULATE] Connected (mock mode)")
        return True
    
    def disconnect(self):
        self.connected = False
        print("[SIMULATE] Disconnected")
    
    def get_status(self) -> Optional[Dict[str, Any]]:
        return {
            "status": "ok",
            "version": "3.1-sim",
            "uptime_s": 12345,
            "servos_enabled": True,
            "eyes_connected": True,
            "distance_sensor": True
        }
    
    def set_servo(self, channel: int, us: int) -> bool:
        return True
    
    def set_all_servos(self, us_values: List[int]) -> bool:
        return True
    
    def all_neutral(self) -> bool:
        return True
    
    def estop(self) -> bool:
        return True
    
    def eye_look(self, x: float, y: float) -> bool:
        return True
    
    def eye_blink(self) -> bool:
        return True
    
    def eye_wink(self, eye: str = "left") -> bool:
        return True
    
    def eye_mood(self, mood: str) -> bool:
        return True
    
    def eye_idle(self, enabled: bool = True) -> bool:
        return True
    
    def get_distance(self) -> Optional[int]:
        import random
        return self._mock_distance + random.randint(-self._distance_variance, self._distance_variance)
    
    def scan_set(self, us: int) -> bool:
        return True
    
    def scan_angle(self, angle_deg: float) -> bool:
        return True
    
    def scan_center(self) -> bool:
        return True
    
    def scan_left(self, angle: float = 45.0) -> bool:
        return True
    
    def scan_right(self, angle: float = 45.0) -> bool:
        return True
    
    def scan_sweep(self, min_angle: float = 0.0, max_angle: float = 180.0,
                   steps: int = 9, delay_s: float = 0.15) -> List[Optional[int]]:
        import random
        readings = []
        for i in range(steps):
            dist = self._mock_distance + random.randint(-100, 100)
            readings.append(max(50, dist))
        return readings
    
    def __enter__(self):
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False


class HardwareTestSuite:
    """Master hardware test suite for Spider Robot v3.1."""
    
    def __init__(self, client, verbose: bool = True):
        self.client = client
        self.verbose = verbose
        self.results: List[TestResult] = []
    
    def log(self, msg: str, end: str = "\n"):
        if self.verbose:
            print(msg, end=end, flush=True)
    
    def add_result(self, name: str, passed: bool, message: str = "", details: Optional[Dict] = None):
        result = TestResult(name, passed, message, details or {})
        self.results.append(result)
        return result
    
    # =========================================================================
    # Connection Tests
    # =========================================================================
    
    def test_connection(self) -> bool:
        """Test connection to Brain daemon and verify WebSocket communication."""
        self.log("\n" + "=" * 60)
        self.log("CONNECTION TEST")
        self.log("=" * 60)
        
        # Test 1: Connection established
        self.log("  Checking connection...", end=" ")
        if self.client.connected:
            self.log("OK")
            self.add_result("Connection Established", True)
        else:
            self.log("FAILED")
            self.add_result("Connection Established", False, "Not connected to Brain daemon")
            return False
        
        # Test 2: Status query
        self.log("  Querying status...", end=" ")
        status = self.client.get_status()
        if status:
            self.log("OK")
            self.log(f"    Response: {json.dumps(status, indent=4)}")
            self.add_result("Status Query", True, details=status)
        else:
            self.log("No response (may be OK)")
            self.add_result("Status Query", True, "No response received (acceptable)")
        
        self.log("  Connection test: PASSED")
        return True
    
    # =========================================================================
    # Servo Tests
    # =========================================================================
    
    def test_servos(self) -> bool:
        """Test all leg servos and scan servo."""
        self.log("\n" + "=" * 60)
        self.log("SERVO TESTS")
        self.log("=" * 60)
        
        all_passed = True
        failed_channels = []
        
        # Test 1: All servos to neutral
        self.log("\n  [1/3] Setting all servos to neutral...", end=" ")
        try:
            if self.client.all_neutral():
                self.log("OK")
                time.sleep(0.5)
                self.add_result("All Servos Neutral", True)
            else:
                self.log("FAILED")
                self.add_result("All Servos Neutral", False)
                all_passed = False
        except Exception as e:
            self.log(f"ERROR: {e}")
            self.add_result("All Servos Neutral", False, str(e))
            all_passed = False
        
        # Test 2: Individual leg servo tests (80° -> 100° -> 90°)
        self.log("\n  [2/3] Testing individual leg servos...")
        
        for leg in range(4):
            leg_name = self.client.LEG_NAMES[leg]
            channels = self.client.LEG_CHANNELS[leg]
            self.log(f"    Leg {leg} ({leg_name}):")
            
            for joint_idx, channel in enumerate(channels):
                joint_name = self.client.JOINT_NAMES[joint_idx]
                self.log(f"      CH{channel} ({joint_name}): ", end="")
                
                try:
                    # Convert degrees to microseconds: 80° -> 1389us, 100° -> 1611us, 90° -> 1500us
                    positions = [
                        (1389, "80°"),   # 80 degrees
                        (1611, "100°"),  # 100 degrees
                        (1500, "90°"),   # 90 degrees (neutral)
                    ]
                    
                    for us, _ in positions:
                        self.client.set_servo(channel, us)
                        time.sleep(0.15)
                    
                    self.log("OK")
                    self.add_result(f"Servo CH{channel} ({leg_name}/{joint_name})", True)
                except Exception as e:
                    self.log(f"FAILED ({e})")
                    failed_channels.append(channel)
                    self.add_result(f"Servo CH{channel} ({leg_name}/{joint_name})", False, str(e))
                    all_passed = False
        
        # Test 3: Scan servo sweep
        self.log("\n  [3/3] Testing scan servo (CH12)...", end=" ")
        try:
            self.client.scan_center()
            time.sleep(0.2)
            self.client.scan_left(45)
            time.sleep(0.2)
            self.client.scan_right(45)
            time.sleep(0.2)
            self.client.scan_center()
            time.sleep(0.2)
            self.log("OK")
            self.add_result("Scan Servo Sweep", True)
        except Exception as e:
            self.log(f"FAILED ({e})")
            self.add_result("Scan Servo Sweep", False, str(e))
            all_passed = False
        
        # Report unresponsive channels
        if failed_channels:
            self.log(f"\n  WARNING: Unresponsive channels: {failed_channels}")
        
        # Return to neutral
        self.client.all_neutral()
        
        self.log(f"\n  Servo test: {'PASSED' if all_passed else 'FAILED'}")
        return all_passed
    
    # =========================================================================
    # Eye Tests
    # =========================================================================
    
    def test_eyes(self) -> bool:
        """Test eye control - moods, look directions, blink/wink."""
        self.log("\n" + "=" * 60)
        self.log("EYE TESTS")
        self.log("=" * 60)
        
        all_passed = True
        
        # Test 1: Moods
        self.log("\n  [1/3] Testing moods...")
        moods = ["normal", "angry", "happy", "sleepy", "normal"]
        for mood in moods:
            self.log(f"    Setting mood: {mood}...", end=" ")
            try:
                if self.client.eye_mood(mood):
                    self.log("OK")
                    time.sleep(0.3)
                else:
                    self.log("FAILED")
                    all_passed = False
            except Exception as e:
                self.log(f"ERROR: {e}")
                all_passed = False
        self.add_result("Eye Moods", all_passed)
        
        # Test 2: Look directions
        self.log("\n  [2/3] Testing look directions...")
        directions = [
            (0, 0, "center"),
            (-1, 0, "left"),
            (1, 0, "right"),
            (0, -1, "up"),
            (0, 1, "down"),
            (-0.7, -0.7, "upper-left"),
            (0.7, -0.7, "upper-right"),
            (-0.7, 0.7, "lower-left"),
            (0.7, 0.7, "lower-right"),
            (0, 0, "center"),
        ]
        
        look_passed = True
        for x, y, name in directions:
            self.log(f"    Looking {name}...", end=" ")
            try:
                if self.client.eye_look(x, y):
                    self.log("OK")
                    time.sleep(0.15)
                else:
                    self.log("FAILED")
                    look_passed = False
            except Exception as e:
                self.log(f"ERROR: {e}")
                look_passed = False
        
        self.add_result("Eye Look Directions", look_passed)
        if not look_passed:
            all_passed = False
        
        # Test 3: Blink and wink
        self.log("\n  [3/3] Testing blink/wink...")
        
        blink_passed = True
        
        self.log("    Blink...", end=" ")
        try:
            if self.client.eye_blink():
                self.log("OK")
                time.sleep(0.5)
            else:
                self.log("FAILED")
                blink_passed = False
        except Exception as e:
            self.log(f"ERROR: {e}")
            blink_passed = False
        
        self.log("    Wink left...", end=" ")
        try:
            if self.client.eye_wink("left"):
                self.log("OK")
                time.sleep(0.5)
            else:
                self.log("FAILED")
                blink_passed = False
        except Exception as e:
            self.log(f"ERROR: {e}")
            blink_passed = False
        
        self.log("    Wink right...", end=" ")
        try:
            if self.client.eye_wink("right"):
                self.log("OK")
                time.sleep(0.5)
            else:
                self.log("FAILED")
                blink_passed = False
        except Exception as e:
            self.log(f"ERROR: {e}")
            blink_passed = False
        
        self.add_result("Eye Blink/Wink", blink_passed)
        if not blink_passed:
            all_passed = False
        
        # Reset to normal
        self.client.eye_mood("normal")
        self.client.eye_look(0, 0)
        
        self.log(f"\n  Eye test: {'PASSED' if all_passed else 'FAILED'}")
        return all_passed
    
    # =========================================================================
    # Distance Sensor Tests
    # =========================================================================
    
    def test_distance(self) -> bool:
        """Test VL53L0X distance sensor with 10 readings."""
        self.log("\n" + "=" * 60)
        self.log("DISTANCE SENSOR TEST")
        self.log("=" * 60)
        
        self.log("\n  Taking 10 readings...")
        
        readings = []
        errors = 0
        
        for i in range(10):
            self.log(f"    Reading {i + 1}/10...", end=" ")
            try:
                dist = self.client.get_distance()
                if dist is not None:
                    readings.append(dist)
                    self.log(f"{dist} mm")
                else:
                    errors += 1
                    self.log("ERROR (no response)")
                time.sleep(0.1)
            except Exception as e:
                errors += 1
                self.log(f"ERROR: {e}")
        
        # Calculate statistics
        if readings:
            min_dist = min(readings)
            max_dist = max(readings)
            avg_dist = sum(readings) / len(readings)
            
            self.log("\n  Results:")
            self.log(f"    Successful readings: {len(readings)}/10")
            self.log(f"    Errors: {errors}")
            self.log(f"    Min: {min_dist} mm")
            self.log(f"    Max: {max_dist} mm")
            self.log(f"    Avg: {avg_dist:.1f} mm")
            self.log(f"    Range variance: {max_dist - min_dist} mm")
            
            passed = len(readings) >= 5  # At least 50% success rate
            self.add_result("Distance Sensor", passed, details={
                "readings": len(readings),
                "errors": errors,
                "min_mm": min_dist,
                "max_mm": max_dist,
                "avg_mm": avg_dist
            })
        else:
            self.log("\n  ERROR: No successful readings!")
            passed = False
            self.add_result("Distance Sensor", False, "No successful readings")
        
        self.log(f"\n  Distance sensor test: {'PASSED' if passed else 'FAILED'}")
        return passed
    
    # =========================================================================
    # Coordinated System Test
    # =========================================================================
    
    def test_coordinated(self) -> bool:
        """Coordinated test: scan servo moves while reading distance, eyes react."""
        self.log("\n" + "=" * 60)
        self.log("COORDINATED SYSTEM TEST")
        self.log("=" * 60)
        
        self.log("\n  Scanning environment with eye reactions...")
        
        all_passed = True
        
        try:
            # Set eyes to normal, looking center
            self.client.eye_mood("normal")
            self.client.eye_look(0, 0)
            
            # Perform sweep with distance readings
            angles = [0, 45, 90, 135, 180]
            
            for angle in angles:
                # Move scan servo
                self.client.scan_angle(angle)
                time.sleep(0.3)
                
                # Read distance
                dist = self.client.get_distance()
                
                # Calculate normalized look direction (map angle to x: -1 to 1)
                look_x = (angle - 90) / 90.0  # 0° -> -1, 90° -> 0, 180° -> 1
                self.client.eye_look(look_x, 0)
                
                # Set mood based on distance
                if dist is not None:
                    if dist < 200:
                        mood = "angry"
                        mood_desc = "CLOSE - angry"
                    elif dist < 500:
                        mood = "normal"
                        mood_desc = "medium - normal"
                    else:
                        mood = "happy"
                        mood_desc = "FAR - happy"
                    
                    self.client.eye_mood(mood)
                    self.log(f"    Angle {angle:3d}°: {dist:4d}mm -> {mood_desc}")
                else:
                    self.log(f"    Angle {angle:3d}°: --error--")
                
                time.sleep(0.2)
            
            # Return to center
            self.client.scan_center()
            self.client.eye_mood("normal")
            self.client.eye_look(0, 0)
            
            self.add_result("Coordinated System Test", True)
            
        except Exception as e:
            self.log(f"\n  ERROR: {e}")
            self.add_result("Coordinated System Test", False, str(e))
            all_passed = False
        
        self.log(f"\n  Coordinated test: {'PASSED' if all_passed else 'FAILED'}")
        return all_passed
    
    # =========================================================================
    # Report
    # =========================================================================
    
    def print_report(self):
        """Print final test summary."""
        self.log("\n" + "=" * 60)
        self.log("TEST REPORT")
        self.log("=" * 60)
        
        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if not r.passed)
        total = len(self.results)
        
        self.log(f"\n  Total Tests: {total}")
        self.log(f"  Passed:      {passed}")
        self.log(f"  Failed:      {failed}")
        self.log("")
        
        # List all results
        self.log("  Details:")
        self.log("  " + "-" * 56)
        
        for result in self.results:
            status = "PASS" if result.passed else "FAIL"
            icon = "+" if result.passed else "X"
            self.log(f"    [{status}] {icon} {result.name}")
            if result.message and not result.passed:
                self.log(f"           -> {result.message}")
        
        self.log("  " + "-" * 56)
        
        if failed == 0:
            self.log("\n  *** ALL TESTS PASSED ***")
        else:
            self.log(f"\n  !!! {failed} TEST(S) FAILED !!!")
        
        self.log("=" * 60 + "\n")
        
        return failed == 0
    
    def run_all(self) -> bool:
        """Run full system test suite."""
        self.test_connection()
        self.test_servos()
        self.test_eyes()
        self.test_distance()
        self.test_coordinated()
        return self.print_report()


def main():
    parser = argparse.ArgumentParser(
        description="Spider Robot v3.1 - Master Hardware Test Suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python test_hardware.py                     # Full system test
  python test_hardware.py --test-servos       # Test servos only
  python test_hardware.py --test-eyes         # Test eyes only
  python test_hardware.py --test-distance     # Test distance sensor only
  python test_hardware.py --simulate          # Run with mock hardware
  python test_hardware.py --simulate --test-servos  # Mock servo test
"""
    )
    parser.add_argument(
        "--ip",
        default="192.168.42.1",
        help="Brain daemon IP address (default: 192.168.42.1)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=9000,
        help="Brain daemon port (default: 9000)"
    )
    parser.add_argument(
        "--test-servos",
        action="store_true",
        help="Run servo tests only"
    )
    parser.add_argument(
        "--test-eyes",
        action="store_true",
        help="Run eye tests only"
    )
    parser.add_argument(
        "--test-distance",
        action="store_true",
        help="Run distance sensor tests only"
    )
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="Run in simulation mode with mock hardware responses"
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Reduce output verbosity"
    )
    
    args = parser.parse_args()
    
    # Determine which tests to run
    run_specific = args.test_servos or args.test_eyes or args.test_distance
    
    # Create client (real or mock)
    if args.simulate:
        print("\n" + "=" * 60)
        print("SPIDER ROBOT v3.1 - HARDWARE TEST SUITE")
        print("MODE: SIMULATION (mock hardware)")
        print("=" * 60)
        client = MockSpiderClient(host=args.ip, port=args.port)
    else:
        print("\n" + "=" * 60)
        print("SPIDER ROBOT v3.1 - HARDWARE TEST SUITE")
        print("MODE: HARDWARE")
        print("=" * 60)
        try:
            from spider_client import SpiderClient
            client = SpiderClient(host=args.ip, port=args.port)
        except ImportError as e:
            print(f"ERROR: Cannot import SpiderClient: {e}")
            print("Make sure spider_client.py is in the same directory.")
            return 1
    
    # Connect
    if not client.connect():
        print(f"\nFailed to connect to Brain daemon at {args.ip}:{args.port}")
        print("\nCheck:")
        print(f"  1. Brain daemon is running on {args.ip}:{args.port}")
        print(f"  2. Network connectivity to {args.ip}")
        print(f"  3. No firewall blocking port {args.port}")
        return 1
    
    # Create test suite
    suite = HardwareTestSuite(client, verbose=not args.quiet)
    
    try:
        # Run connection test first (always)
        suite.test_connection()
        
        if run_specific:
            # Run specific tests
            if args.test_servos:
                suite.test_servos()
            if args.test_eyes:
                suite.test_eyes()
            if args.test_distance:
                suite.test_distance()
        else:
            # Run full system test
            suite.test_servos()
            suite.test_eyes()
            suite.test_distance()
            suite.test_coordinated()
        
        # Print report
        all_passed = suite.print_report()
        
        return 0 if all_passed else 1
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        print("Sending E-STOP for safety...")
        try:
            client.estop()
        except Exception:
            pass
        return 1
    finally:
        # Cleanup
        try:
            client.all_neutral()
        except Exception:
            pass
        client.disconnect()


if __name__ == "__main__":
    sys.exit(main())
