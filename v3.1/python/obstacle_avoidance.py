#!/usr/bin/env python3
"""
Spider Robot v3.1 - Obstacle Avoidance Module

Reusable obstacle detection and avoidance logic.
Works with VL53L0X distance sensor on scan servo.

Usage:
    from obstacle_avoidance import ObstacleAvoidance, Action
    
    oa = ObstacleAvoidance(client)
    action = oa.get_recommended_action()
    
    if action == Action.FORWARD:
        gait.step(Direction.FORWARD)
    elif action == Action.TURN_LEFT:
        gait.step(Direction.ROTATE_CCW)
    ...
"""

import time
import logging
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Callable

try:
    from spider_client import SpiderClient
except ImportError:
    SpiderClient = None

logger = logging.getLogger(__name__)


class Action(Enum):
    """Recommended navigation action based on obstacle detection."""
    FORWARD = auto()       # Path clear, full speed ahead
    SLOW_FORWARD = auto()  # Obstacle detected but not critical
    TURN_LEFT = auto()     # Better clearance on left
    TURN_RIGHT = auto()    # Better clearance on right
    BACKUP = auto()        # Obstacle very close, need to reverse
    STOP = auto()          # No safe direction found


@dataclass
class ScanData:
    """Results from an environment scan."""
    angles: List[float] = field(default_factory=list)
    distances: List[Optional[int]] = field(default_factory=list)
    timestamp: float = 0.0
    
    @property
    def valid_readings(self) -> List[Tuple[float, int]]:
        """Return only valid (non-None) readings as (angle, distance) pairs."""
        return [(a, d) for a, d in zip(self.angles, self.distances) if d is not None]
    
    @property
    def min_distance(self) -> Optional[int]:
        """Return the minimum distance from all readings."""
        valid = [d for d in self.distances if d is not None]
        return min(valid) if valid else None
    
    @property
    def max_distance(self) -> Optional[int]:
        """Return the maximum distance from all readings."""
        valid = [d for d in self.distances if d is not None]
        return max(valid) if valid else None
    
    def get_distance_at_angle(self, target_angle: float) -> Optional[int]:
        """Get distance reading closest to a given angle."""
        if not self.angles:
            return None
        closest_idx = min(range(len(self.angles)), 
                         key=lambda i: abs(self.angles[i] - target_angle))
        return self.distances[closest_idx]
    
    def get_center_distance(self) -> Optional[int]:
        """Get distance reading at center (90°)."""
        return self.get_distance_at_angle(90.0)


class ObstacleAvoidance:
    """
    Obstacle detection and avoidance controller.
    
    Uses VL53L0X sensor on scan servo to detect obstacles and
    recommend navigation actions.
    
    Distance thresholds (mm):
    - critical_distance: Stop immediately, back up
    - warning_distance: Slow down, prepare to turn
    - safe_distance: Full speed ok
    
    Scan angles (degrees, viewed from top):
    - 0° = full right
    - 90° = center/forward
    - 180° = full left
    """
    
    # Default scan parameters
    DEFAULT_SCAN_MIN_ANGLE = 30.0   # Don't scan behind legs
    DEFAULT_SCAN_MAX_ANGLE = 150.0
    DEFAULT_SCAN_STEPS = 9
    DEFAULT_SCAN_DELAY = 0.12       # Servo settle time
    
    def __init__(self, client: 'SpiderClient'):
        """
        Initialize obstacle avoidance controller.
        
        Args:
            client: SpiderClient instance for hardware communication
        """
        self.client = client
        
        # Distance thresholds (mm)
        self.critical_distance = 120  # Stop immediately
        self.warning_distance = 250   # Slow down
        self.safe_distance = 400      # Full speed ok
        
        # Scan parameters
        self.scan_min_angle = self.DEFAULT_SCAN_MIN_ANGLE
        self.scan_max_angle = self.DEFAULT_SCAN_MAX_ANGLE
        self.scan_steps = self.DEFAULT_SCAN_STEPS
        self.scan_delay = self.DEFAULT_SCAN_DELAY
        
        # State
        self._last_scan: Optional[ScanData] = None
        self._sensor_available = True
        self._simulate = False
        
        # Eye integration callbacks
        self._on_scan_angle: Optional[Callable[[float], None]] = None
        self._on_obstacle_detected: Optional[Callable[[int], None]] = None
        self._on_path_clear: Optional[Callable[[], None]] = None
    
    @property
    def last_scan(self) -> Optional[ScanData]:
        """Get the most recent scan data."""
        return self._last_scan
    
    def set_simulate(self, enabled: bool = True):
        """Enable simulation mode (no hardware required)."""
        self._simulate = enabled
        if enabled:
            logger.info("Obstacle avoidance running in simulation mode")
    
    def set_eye_callbacks(self, 
                          on_scan_angle: Optional[Callable[[float], None]] = None,
                          on_obstacle_detected: Optional[Callable[[int], None]] = None,
                          on_path_clear: Optional[Callable[[], None]] = None):
        """
        Set callbacks for eye integration.
        
        Args:
            on_scan_angle: Called with angle during scan sweep (for eye tracking)
            on_obstacle_detected: Called with distance when obstacle detected
            on_path_clear: Called when path is clear
        """
        self._on_scan_angle = on_scan_angle
        self._on_obstacle_detected = on_obstacle_detected
        self._on_path_clear = on_path_clear
    
    def _get_distance(self) -> Optional[int]:
        """Get distance reading with graceful degradation."""
        if self._simulate:
            import random
            # Simulate with occasional obstacles
            if random.random() < 0.1:
                return random.randint(80, 200)
            return random.randint(400, 1200)
        
        if not self._sensor_available:
            return None
        
        try:
            distance = self.client.get_distance()
            if distance is None:
                logger.debug("Distance sensor returned None")
            return distance
        except Exception as e:
            logger.warning(f"Distance sensor error: {e}")
            self._sensor_available = False
            return None
    
    def _set_scan_angle(self, angle: float):
        """Set scan servo angle with graceful degradation."""
        if self._simulate:
            return
        
        try:
            self.client.scan_angle(angle)
        except Exception as e:
            logger.warning(f"Scan servo error: {e}")
    
    def check_front(self) -> Tuple[bool, Optional[int]]:
        """
        Quick check of distance directly ahead.
        
        Returns:
            Tuple of (is_safe, distance_mm)
            is_safe is True if distance > critical_distance or sensor unavailable
        """
        if not self._simulate:
            self._set_scan_angle(90.0)  # Center
            time.sleep(self.scan_delay)
        
        distance = self._get_distance()
        
        if distance is None:
            # Sensor unavailable - assume safe but log warning
            logger.warning("Distance sensor unavailable, assuming safe")
            return (True, None)
        
        is_safe = distance > self.critical_distance
        
        if not is_safe and self._on_obstacle_detected:
            self._on_obstacle_detected(distance)
        elif is_safe and distance > self.safe_distance and self._on_path_clear:
            self._on_path_clear()
        
        return (is_safe, distance)
    
    def scan_environment(self, angles: int = None) -> ScanData:
        """
        Perform a sweep scan to build an obstacle map.
        
        Args:
            angles: Number of scan positions (default: scan_steps)
            
        Returns:
            ScanData with angles and distance readings
        """
        num_steps = angles if angles is not None else self.scan_steps
        
        scan_data = ScanData(timestamp=time.time())
        
        if num_steps < 2:
            # Single reading at center
            scan_data.angles = [90.0]
            _, dist = self.check_front()
            scan_data.distances = [dist]
            self._last_scan = scan_data
            return scan_data
        
        step_size = (self.scan_max_angle - self.scan_min_angle) / (num_steps - 1)
        
        for i in range(num_steps):
            angle = self.scan_min_angle + i * step_size
            scan_data.angles.append(angle)
            
            # Move scan servo
            self._set_scan_angle(angle)
            
            # Eye tracking callback
            if self._on_scan_angle:
                self._on_scan_angle(angle)
            
            time.sleep(self.scan_delay if not self._simulate else 0.02)
            
            # Read distance
            distance = self._get_distance()
            scan_data.distances.append(distance)
            
            logger.debug(f"Scan {angle:.1f}°: {distance}mm")
        
        # Return to center
        if not self._simulate:
            self._set_scan_angle(90.0)
        
        self._last_scan = scan_data
        return scan_data
    
    def find_best_direction(self, scan_data: Optional[ScanData] = None) -> float:
        """
        Analyze scan data to find the best heading angle.
        
        Uses a scoring algorithm that considers:
        - Distance at each angle (higher = better)
        - Neighboring readings (smooth paths preferred)
        - Slight preference for center (less turning needed)
        
        Args:
            scan_data: Scan data to analyze (default: last scan)
            
        Returns:
            Best heading angle in degrees (0=right, 90=center, 180=left)
        """
        data = scan_data or self._last_scan
        
        if data is None or not data.angles:
            logger.warning("No scan data available, defaulting to center")
            return 90.0
        
        best_idx = 0
        best_score = float('-inf')
        
        for i, dist in enumerate(data.distances):
            if dist is None:
                continue
            
            # Base score from distance
            score = float(dist)
            
            # Bonus for wide clearance (check neighbors)
            if i > 0 and data.distances[i-1] is not None:
                score += data.distances[i-1] * 0.25
            if i < len(data.distances) - 1 and data.distances[i+1] is not None:
                score += data.distances[i+1] * 0.25
            
            # Small penalty for deviation from center (prefer going straight)
            center_idx = len(data.angles) // 2
            deviation_penalty = abs(i - center_idx) * 15
            score -= deviation_penalty
            
            if score > best_score:
                best_score = score
                best_idx = i
        
        best_angle = data.angles[best_idx] if data.angles else 90.0
        logger.debug(f"Best direction: {best_angle:.1f}° (score: {best_score:.0f})")
        
        return best_angle
    
    def get_recommended_action(self, use_cached_scan: bool = False) -> Action:
        """
        Get recommended navigation action based on current sensor data.
        
        Args:
            use_cached_scan: If True, use last scan data instead of new scan
            
        Returns:
            Recommended Action enum value
        """
        # Quick front check first
        is_safe, front_distance = self.check_front()
        
        if front_distance is None:
            # Sensor unavailable - assume safe
            logger.warning("Sensor unavailable, recommending cautious forward")
            return Action.SLOW_FORWARD
        
        # Critical distance - need to back up
        if front_distance <= self.critical_distance:
            logger.info(f"CRITICAL: Obstacle at {front_distance}mm, backing up")
            if self._on_obstacle_detected:
                self._on_obstacle_detected(front_distance)
            return Action.BACKUP
        
        # Warning distance - need to scan and decide
        if front_distance <= self.warning_distance:
            logger.info(f"WARNING: Obstacle at {front_distance}mm, scanning...")
            
            # Do a full scan to find best direction
            if use_cached_scan and self._last_scan:
                scan = self._last_scan
            else:
                scan = self.scan_environment()
            
            best_angle = self.find_best_direction(scan)
            best_distance = scan.get_distance_at_angle(best_angle)
            
            # Check if best direction is clear enough
            if best_distance is None or best_distance <= self.critical_distance:
                logger.warning("No safe direction found")
                return Action.STOP
            
            if best_distance <= self.warning_distance:
                # Even best direction has obstacle
                if best_angle > 100:  # Left is better
                    return Action.TURN_LEFT
                elif best_angle < 80:  # Right is better
                    return Action.TURN_RIGHT
                else:
                    return Action.BACKUP
            
            # Best direction is reasonably clear
            if best_angle > 100:
                return Action.TURN_LEFT
            elif best_angle < 80:
                return Action.TURN_RIGHT
            else:
                return Action.SLOW_FORWARD
        
        # Safe distance but not fully clear
        if front_distance <= self.safe_distance:
            if self._on_path_clear:
                self._on_path_clear()
            return Action.SLOW_FORWARD
        
        # Path is clear
        if self._on_path_clear:
            self._on_path_clear()
        return Action.FORWARD
    
    def get_turn_direction_from_scan(self) -> Tuple[Action, float]:
        """
        Analyze last scan and return turn direction with confidence.
        
        Returns:
            Tuple of (Action, confidence 0-1)
            Action is TURN_LEFT, TURN_RIGHT, or FORWARD
        """
        if self._last_scan is None:
            return (Action.FORWARD, 0.0)
        
        # Split scan into left and right halves
        left_distances = []
        right_distances = []
        
        for angle, dist in zip(self._last_scan.angles, self._last_scan.distances):
            if dist is None:
                continue
            if angle > 90:
                left_distances.append(dist)
            else:
                right_distances.append(dist)
        
        left_avg = sum(left_distances) / len(left_distances) if left_distances else 0
        right_avg = sum(right_distances) / len(right_distances) if right_distances else 0
        
        total = left_avg + right_avg
        if total == 0:
            return (Action.FORWARD, 0.0)
        
        if left_avg > right_avg * 1.2:  # Left is 20% better
            confidence = min(1.0, (left_avg - right_avg) / self.safe_distance)
            return (Action.TURN_LEFT, confidence)
        elif right_avg > left_avg * 1.2:
            confidence = min(1.0, (right_avg - left_avg) / self.safe_distance)
            return (Action.TURN_RIGHT, confidence)
        else:
            return (Action.FORWARD, 0.5)
    
    def integrate_with_eyes(self, client: 'SpiderClient' = None):
        """
        Set up automatic eye integration.
        
        Eyes will:
        - Look in scan direction during sweep
        - Show angry expression when obstacle detected
        - Show happy expression when path is clear
        """
        c = client or self.client
        
        def on_scan_angle(angle: float):
            # Convert scan angle to eye look position
            # 90° = center = (0, 0)
            # 30° = right = (0.7, 0)
            # 150° = left = (-0.7, 0)
            x = -(angle - 90) / 90.0  # Invert: left scan = left look
            x = max(-0.8, min(0.8, x))
            try:
                c.eye_look(x, 0.1)  # Slight downward gaze
            except Exception:
                pass
        
        def on_obstacle_detected(distance: int):
            try:
                c.eye_mood("angry")
            except Exception:
                pass
        
        def on_path_clear():
            try:
                c.eye_mood("happy")
            except Exception:
                pass
        
        self.set_eye_callbacks(on_scan_angle, on_obstacle_detected, on_path_clear)
        logger.info("Eye integration enabled")
    
    def format_scan_ascii(self, scan_data: Optional[ScanData] = None) -> str:
        """
        Generate ASCII visualization of scan data.
        
        Args:
            scan_data: Scan data to visualize (default: last scan)
            
        Returns:
            Multi-line ASCII art string
        """
        data = scan_data or self._last_scan
        
        if data is None or not data.angles:
            return "No scan data available"
        
        lines = []
        lines.append("Scan Result:")
        lines.append("-" * 45)
        
        max_dist = data.max_distance or 1000
        
        for angle, dist in zip(data.angles, data.distances):
            if dist is None:
                bar = "??? (no reading)"
            else:
                bar_len = int((dist / max_dist) * 25)
                bar = "#" * bar_len
                
                # Add threshold markers
                if dist <= self.critical_distance:
                    status = " !! CRITICAL"
                elif dist <= self.warning_distance:
                    status = " ! WARNING"
                elif dist <= self.safe_distance:
                    status = " ~ CAUTION"
                else:
                    status = ""
                bar += status
            
            direction = "L" if angle > 95 else ("R" if angle < 85 else "C")
            lines.append(f"{angle:6.1f}°[{direction}]: {dist if dist else '---':>5} {bar}")
        
        lines.append("-" * 45)
        
        best_angle = self.find_best_direction(data)
        best_dist = data.get_distance_at_angle(best_angle)
        lines.append(f"Best direction: {best_angle:.1f}° @ {best_dist}mm")
        
        return "\n".join(lines)


def create_obstacle_avoider(host: str = "192.168.42.1", port: int = 9000, 
                           simulate: bool = False) -> Tuple[Optional['SpiderClient'], ObstacleAvoidance]:
    """
    Factory function to create obstacle avoidance setup.
    
    Args:
        host: Spider Brain IP address
        port: Spider Brain port
        simulate: If True, don't require real hardware
        
    Returns:
        Tuple of (client, obstacle_avoidance)
        client may be None if simulate=True and connection fails
    """
    if SpiderClient is None:
        raise ImportError("spider_client module not found")
    
    client = SpiderClient(host=host, port=port)
    
    if not simulate:
        if not client.connect():
            raise ConnectionError(f"Failed to connect to Spider Brain at {host}:{port}")
    else:
        # Try to connect but don't fail if it doesn't work
        try:
            client.connect()
        except Exception:
            pass
    
    oa = ObstacleAvoidance(client)
    oa.set_simulate(simulate)
    
    return (client, oa)
