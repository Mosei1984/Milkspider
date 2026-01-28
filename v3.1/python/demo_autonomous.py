#!/usr/bin/env python3
"""
Spider Robot v3.1 - Autonomous Navigation Demo (Enhanced)

Scan-based navigation with state machine, eye integration, and smart obstacle handling.
Uses VL53L0X sensor on scan servo to build distance maps and navigate intelligently.

States:
- SCANNING: Sweep scan servo, analyze readings to find best direction
- MOVING: Walk forward with periodic distance checks
- TURNING: Rotate towards best direction
- OBSTACLE: Stop, back up, re-scan
- IDLE: Standing still, looking around

Usage:
    python demo_autonomous.py [--ip 192.168.42.1] [--duration 60] [--speed 1.0] [--verbose]
"""

import argparse
import time
import random
import math
import sys
from enum import Enum, auto
from dataclasses import dataclass
from typing import List, Tuple, Optional

try:
    from calibrated_client import CalibratedSpiderClient as SpiderClient
    USING_CALIBRATED = True
except ImportError:
    from spider_client import SpiderClient
    USING_CALIBRATED = False

from gait_library import GaitGenerator, GaitType, Direction


class RobotState(Enum):
    IDLE = auto()
    SCANNING = auto()
    MOVING = auto()
    TURNING = auto()
    OBSTACLE = auto()


@dataclass
class ScanResult:
    """Result of a distance scan sweep."""
    angles: List[float]
    distances: List[Optional[int]]
    best_angle: float
    best_distance: int
    clearance_score: float
    
    def is_clear_ahead(self, threshold_mm: int = 400) -> bool:
        """Check if the center direction is clear."""
        center_idx = len(self.distances) // 2
        center_dist = self.distances[center_idx]
        return center_dist is not None and center_dist > threshold_mm
    
    def get_direction_clearance(self, angle: float) -> Optional[int]:
        """Get distance reading closest to a given angle."""
        if not self.angles:
            return None
        closest_idx = min(range(len(self.angles)), 
                         key=lambda i: abs(self.angles[i] - angle))
        return self.distances[closest_idx]


class AutonomousController:
    """Enhanced autonomous navigation controller with scan-based navigation."""

    DIST_CRITICAL = 120
    DIST_CLOSE = 200
    DIST_MEDIUM = 400
    DIST_FAR = 800
    DIST_CLEAR = 1200

    SCAN_STEPS = 9
    SCAN_MIN_ANGLE = 30.0
    SCAN_MAX_ANGLE = 150.0
    SCAN_DELAY = 0.12

    STEPS_BETWEEN_SCANS = 4
    STEPS_BETWEEN_CHECKS = 2

    def __init__(self, client: SpiderClient, verbose: bool = False):
        self.client = client
        self.gait = GaitGenerator(client)
        self.gait.set_gait(GaitType.TRIPOD)
        
        self.state = RobotState.IDLE
        self.running = False
        self.verbose = verbose
        
        self.last_scan: Optional[ScanResult] = None
        self.steps_since_scan = 0
        self.steps_since_check = 0
        self.current_heading = 0.0
        
        self.simulate_sensor = True

    def log(self, msg: str):
        """Print message if verbose mode enabled."""
        if self.verbose:
            print(f"  [DEBUG] {msg}")

    def get_distance(self) -> Optional[int]:
        """Get single distance reading from sensor."""
        if self.simulate_sensor:
            if random.random() < 0.08:
                return random.randint(80, 250)
            return random.randint(400, 1500)
        
        return self.client.get_distance()

    def scan_sweep(self) -> ScanResult:
        """
        Perform a full sweep scan to build a distance map.
        
        Returns:
            ScanResult with angles, distances, and analysis.
        """
        self.log(f"Performing sweep scan ({self.SCAN_STEPS} steps)")
        
        angles = []
        distances = []
        
        step_size = (self.SCAN_MAX_ANGLE - self.SCAN_MIN_ANGLE) / max(1, self.SCAN_STEPS - 1)
        
        for i in range(self.SCAN_STEPS):
            angle = self.SCAN_MIN_ANGLE + i * step_size
            angles.append(angle)
            
            if self.simulate_sensor:
                time.sleep(0.05)
                if random.random() < 0.1:
                    dist = random.randint(100, 300)
                else:
                    base = 600 + 400 * math.sin(math.radians(angle))
                    dist = int(base + random.randint(-100, 100))
                distances.append(max(50, dist))
            else:
                self.client.scan_angle(angle)
                time.sleep(self.SCAN_DELAY)
                dist = self.client.get_distance()
                distances.append(dist)
        
        if not self.simulate_sensor:
            self.client.scan_center()
        
        best_idx = 0
        best_score = 0
        
        for i, dist in enumerate(distances):
            if dist is None:
                continue
            
            score = dist
            
            if i > 0 and distances[i-1] is not None:
                score += distances[i-1] * 0.3
            if i < len(distances) - 1 and distances[i+1] is not None:
                score += distances[i+1] * 0.3
            
            center_penalty = abs(i - len(distances) // 2) * 20
            score -= center_penalty
            
            if score > best_score:
                best_score = score
                best_idx = i
        
        best_angle = angles[best_idx] if angles else 90.0
        best_distance = distances[best_idx] if distances[best_idx] else 0
        
        valid_distances = [d for d in distances if d is not None]
        clearance_score = sum(valid_distances) / len(valid_distances) if valid_distances else 0
        
        result = ScanResult(
            angles=angles,
            distances=distances,
            best_angle=best_angle,
            best_distance=best_distance,
            clearance_score=clearance_score
        )
        
        if self.verbose:
            self._print_scan_result(result)
        
        return result

    def _print_scan_result(self, scan: ScanResult):
        """Print a visual representation of the scan."""
        print("\n  Scan Result:")
        print("  " + "-" * 40)
        
        max_dist = max((d for d in scan.distances if d), default=1000)
        
        for i, (angle, dist) in enumerate(zip(scan.angles, scan.distances)):
            if dist is None:
                bar = "???"
            else:
                bar_len = int((dist / max_dist) * 20)
                bar = "█" * bar_len
            
            marker = " <--BEST" if angle == scan.best_angle else ""
            direction = "L" if angle > 90 else ("R" if angle < 90 else "C")
            print(f"  {angle:5.1f}°[{direction}]: {dist if dist else '---':>4} {bar}{marker}")
        
        print("  " + "-" * 40)
        print(f"  Best: {scan.best_angle:.1f}° @ {scan.best_distance}mm, Score: {scan.clearance_score:.0f}")
        print()

    def angle_to_turn_direction(self, angle: float) -> Tuple[Direction, int]:
        """
        Convert scan angle to turn direction and steps.
        
        Args:
            angle: Scan angle (0=right, 90=center, 180=left)
            
        Returns:
            (Direction, steps) tuple
        """
        deviation = angle - 90.0
        
        if abs(deviation) < 15:
            return Direction.FORWARD, 0
        
        steps = max(1, int(abs(deviation) / 30))
        steps = min(steps, 4)
        
        if deviation > 0:
            return Direction.ROTATE_CCW, steps
        else:
            return Direction.ROTATE_CW, steps

    def set_eye_state(self):
        """Update eye expression based on current state."""
        if self.state == RobotState.OBSTACLE:
            self.client.eye_mood("angry")
        elif self.state == RobotState.MOVING:
            self.client.eye_mood("happy")
        elif self.state == RobotState.SCANNING:
            self.client.eye_mood("normal")
        elif self.state == RobotState.TURNING:
            self.client.eye_mood("normal")
        elif self.state == RobotState.IDLE:
            self.client.eye_mood("sleepy")

    def look_direction(self, direction: Direction):
        """Make eyes look in movement direction."""
        if direction == Direction.FORWARD:
            self.client.eye_look(0, 0.2)
        elif direction == Direction.BACKWARD:
            self.client.eye_look(0, -0.3)
        elif direction == Direction.ROTATE_CW:
            self.client.eye_look(0.8, 0)
        elif direction == Direction.ROTATE_CCW:
            self.client.eye_look(-0.8, 0)
        elif direction == Direction.LEFT:
            self.client.eye_look(-0.5, 0)
        elif direction == Direction.RIGHT:
            self.client.eye_look(0.5, 0)

    def handle_scanning(self) -> RobotState:
        """SCANNING state: Perform sweep and analyze."""
        self.log("State: SCANNING")
        self.set_eye_state()
        
        self.client.eye_look(-0.5, 0)
        time.sleep(0.2)
        
        self.last_scan = self.scan_sweep()
        self.steps_since_scan = 0
        
        self.client.eye_look(0.5, 0)
        time.sleep(0.2)
        self.client.eye_look(0, 0)
        
        if self.last_scan.best_distance < self.DIST_CLOSE:
            print(f"  All directions blocked! Best: {self.last_scan.best_distance}mm")
            return RobotState.OBSTACLE
        
        turn_dir, turn_steps = self.angle_to_turn_direction(self.last_scan.best_angle)
        
        if turn_steps > 0:
            print(f"  Best direction: {self.last_scan.best_angle:.0f}° -> turning {turn_steps} steps")
            return RobotState.TURNING
        else:
            print(f"  Path clear ahead ({self.last_scan.best_distance}mm)")
            return RobotState.MOVING

    def handle_moving(self) -> RobotState:
        """MOVING state: Walk forward with periodic checks."""
        self.log("State: MOVING")
        self.state = RobotState.MOVING
        self.set_eye_state()
        self.look_direction(Direction.FORWARD)
        
        if self.steps_since_check >= self.STEPS_BETWEEN_CHECKS:
            if not self.simulate_sensor:
                self.client.scan_center()
            dist = self.get_distance()
            self.steps_since_check = 0
            
            self.log(f"Forward check: {dist}mm")
            
            if dist is not None and dist < self.DIST_CRITICAL:
                print(f"  OBSTACLE! Distance: {dist}mm - stopping immediately")
                return RobotState.OBSTACLE
            elif dist is not None and dist < self.DIST_CLOSE:
                print(f"  Obstacle approaching ({dist}mm) - rescanning")
                return RobotState.SCANNING
        
        self.gait.step(Direction.FORWARD)
        self.steps_since_scan += 1
        self.steps_since_check += 1
        
        if self.steps_since_scan >= self.STEPS_BETWEEN_SCANS:
            return RobotState.SCANNING
        
        if random.random() < 0.03:
            self.client.eye_blink()
        
        return RobotState.MOVING

    def handle_turning(self) -> RobotState:
        """TURNING state: Rotate towards best direction."""
        self.log("State: TURNING")
        self.state = RobotState.TURNING
        self.set_eye_state()
        
        if self.last_scan is None:
            return RobotState.SCANNING
        
        turn_dir, turn_steps = self.angle_to_turn_direction(self.last_scan.best_angle)
        
        if turn_steps == 0:
            return RobotState.MOVING
        
        self.look_direction(turn_dir)
        dir_name = "CCW" if turn_dir == Direction.ROTATE_CCW else "CW"
        print(f"  Turning {dir_name} x{turn_steps}")
        
        for i in range(turn_steps):
            if not self.running:
                break
            self.gait.step(turn_dir)
        
        time.sleep(0.1)
        self.client.eye_look(0, 0)
        
        return RobotState.MOVING

    def handle_obstacle(self) -> RobotState:
        """OBSTACLE state: Back up and rescan."""
        self.log("State: OBSTACLE")
        self.state = RobotState.OBSTACLE
        self.set_eye_state()
        
        print("  Obstacle handling: backing up...")
        self.look_direction(Direction.BACKWARD)
        
        backup_steps = random.randint(1, 2)
        for _ in range(backup_steps):
            if not self.running:
                break
            self.gait.step(Direction.BACKWARD)
        
        time.sleep(0.3)
        self.client.eye_look(0, 0)
        
        return RobotState.SCANNING

    def handle_idle(self) -> RobotState:
        """IDLE state: Standing still, looking around."""
        self.log("State: IDLE")
        self.set_eye_state()
        
        for _ in range(3):
            x = random.uniform(-0.5, 0.5)
            y = random.uniform(-0.3, 0.3)
            self.client.eye_look(x, y)
            time.sleep(random.uniform(0.5, 1.5))
            
            if random.random() < 0.3:
                self.client.eye_blink()
        
        self.client.eye_look(0, 0)
        return RobotState.SCANNING

    def run(self, duration: float = 60.0, speed: float = 1.0):
        """
        Run autonomous navigation for specified duration.
        
        Args:
            duration: Run time in seconds
            speed: Gait speed factor (0.5-2.0)
        """
        print(f"\n{'='*50}")
        print(f"Starting Enhanced Autonomous Navigation")
        print(f"  Duration: {duration}s")
        print(f"  Speed: {speed}x")
        print(f"  Sensor: {'Simulated' if self.simulate_sensor else 'Real VL53L0X'}")
        print(f"  Client: {'CalibratedClient' if USING_CALIBRATED else 'SpiderClient'}")
        print(f"{'='*50}\n")
        
        self.running = True
        self.gait.set_speed(speed)
        start_time = time.time()
        step_count = 0
        
        self.gait.stand()
        time.sleep(0.5)
        self.client.eye_idle(False)
        self.client.eye_mood("happy")
        
        self.state = RobotState.SCANNING
        
        try:
            while self.running and (time.time() - start_time) < duration:
                elapsed = time.time() - start_time
                remaining = duration - elapsed
                
                if self.state != RobotState.MOVING or self.verbose:
                    print(f"[{elapsed:5.1f}s] State: {self.state.name:10} | Remaining: {remaining:.0f}s")
                
                if self.state == RobotState.SCANNING:
                    self.state = self.handle_scanning()
                elif self.state == RobotState.MOVING:
                    prev_state = self.state
                    self.state = self.handle_moving()
                    if prev_state == RobotState.MOVING:
                        step_count += 1
                elif self.state == RobotState.TURNING:
                    self.state = self.handle_turning()
                elif self.state == RobotState.OBSTACLE:
                    self.state = self.handle_obstacle()
                elif self.state == RobotState.IDLE:
                    self.state = self.handle_idle()
        
        except KeyboardInterrupt:
            print("\n\nInterrupted by user!")
        
        finally:
            print(f"\n{'='*50}")
            print(f"Navigation Complete")
            print(f"  Total steps: {step_count}")
            print(f"  Duration: {time.time() - start_time:.1f}s")
            print(f"{'='*50}")
            
            self.running = False
            self.gait.stop()
            self.client.eye_mood("sleepy")
            time.sleep(0.5)
            self.client.eye_idle(True)

    def stop(self):
        """Stop the autonomous controller."""
        self.running = False


def main():
    parser = argparse.ArgumentParser(
        description="Spider Robot v3.1 - Enhanced Autonomous Navigation Demo",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
States:
  SCANNING  - Sweep scan servo, analyze readings
  MOVING    - Walk forward with periodic distance checks
  TURNING   - Rotate towards best direction
  OBSTACLE  - Stop, back up, re-scan
  IDLE      - Standing still, looking around

Examples:
  python demo_autonomous.py --duration 120 --speed 1.5
  python demo_autonomous.py --no-simulate --verbose
        """
    )
    parser.add_argument("--ip", default="192.168.42.1", 
                        help="Brain daemon IP (default: 192.168.42.1)")
    parser.add_argument("--port", type=int, default=9000, 
                        help="Brain daemon port (default: 9000)")
    parser.add_argument("--duration", type=float, default=60.0, 
                        help="Run duration in seconds (default: 60)")
    parser.add_argument("--speed", type=float, default=1.0,
                        help="Gait speed factor 0.5-2.0 (default: 1.0)")
    parser.add_argument("--no-simulate", action="store_true",
                        help="Use real VL53L0X sensor instead of simulation")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print detailed scan data and decisions")
    args = parser.parse_args()

    if not 0.5 <= args.speed <= 2.0:
        print(f"WARNING: Speed {args.speed} out of range, clamping to 0.5-2.0")
        args.speed = max(0.5, min(2.0, args.speed))

    print("Spider Robot v3.1 - Enhanced Autonomous Navigation Demo")
    print("=" * 55)
    print(f"Target: {args.ip}:{args.port}")
    print(f"Duration: {args.duration}s | Speed: {args.speed}x")
    print(f"Client: {'CalibratedSpiderClient' if USING_CALIBRATED else 'SpiderClient'}")
    print()

    client = SpiderClient(host=args.ip, port=args.port)
    if not client.connect():
        print("Failed to connect!")
        return 1

    controller = AutonomousController(client, verbose=args.verbose)
    controller.simulate_sensor = not args.no_simulate

    if controller.simulate_sensor:
        print("NOTE: Using simulated distance sensor")
        print("      Use --no-simulate for real VL53L0X sensor")

    try:
        print("\nPress Enter to start autonomous mode...")
        print("(Press Ctrl+C at any time to stop)")
        input()
        controller.run(args.duration, args.speed)
    except KeyboardInterrupt:
        print("\nAborted!")
        controller.stop()
    finally:
        print("\nReturning to safe position...")
        client.all_neutral()
        time.sleep(0.5)
        client.disconnect()

    print("Demo complete!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
