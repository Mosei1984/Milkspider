#!/usr/bin/env python3
"""
Spider Robot v3.1 - WebSocket Client Library

Simple WebSocket client for communicating with the Spider Brain daemon.
Provides methods for servo control, E-STOP, and status queries.

Protocol: JSON over WebSocket
Default endpoint: ws://192.168.42.1:9000
"""

import json
import time
from typing import Optional, List, Dict, Any

try:
    import websocket
except ImportError:
    print("ERROR: websocket-client not installed. Run: pip install websocket-client")
    raise


class SpiderClient:
    """WebSocket client for Spider Robot v3.1 Brain daemon."""
    
    # Servo configuration
    SERVO_COUNT = 12         # Leg servos only (CH0-11)
    SERVO_COUNT_TOTAL = 13   # All servos including scan (CH0-12)
    SERVO_CHANNEL_SCAN = 12  # Scan servo channel
    SERVO_MIN_US = 500
    SERVO_MAX_US = 2500
    SERVO_NEUTRAL_US = 1500
    
    # Calibration offset limits (degrees)
    CALIB_OFFSET_MIN_DEG = -30
    CALIB_OFFSET_MAX_DEG = 30
    
    # Leg mapping: leg_index -> (coxa, femur, tibia) channel indices
    LEG_CHANNELS = {
        0: (0, 1, 2),    # Front Right (FR)
        1: (3, 4, 5),    # Front Left (FL)
        2: (6, 7, 8),    # Rear Right (RR)
        3: (9, 10, 11),  # Rear Left (RL)
    }
    
    LEG_NAMES = {0: "FR", 1: "FL", 2: "RR", 3: "RL"}
    JOINT_NAMES = {0: "coxa", 1: "femur", 2: "tibia"}
    
    def __init__(self, host: str = "192.168.42.1", port: int = 9000):
        """Initialize client with Brain daemon address."""
        self.host = host
        self.port = port
        self.url = f"ws://{host}:{port}"
        self.ws: Optional[websocket.WebSocket] = None
        self.connected = False
        self.timeout = 5.0
        self._calibration_offsets_us: List[int] = [0] * self.SERVO_COUNT_TOTAL
        self._apply_calibration = False
    
    def connect(self) -> bool:
        """Connect to the Brain daemon. Returns True on success."""
        try:
            print(f"Connecting to {self.url}...")
            self.ws = websocket.create_connection(
                self.url,
                timeout=self.timeout
            )
            self.connected = True
            print(f"Connected to Spider Brain at {self.url}")
            return True
        except ConnectionRefusedError:
            print(f"ERROR: Connection refused. Is Brain daemon running on {self.host}:{self.port}?")
            return False
        except websocket.WebSocketTimeoutException:
            print(f"ERROR: Connection timeout. Check network connectivity to {self.host}")
            return False
        except Exception as e:
            print(f"ERROR: Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the Brain daemon."""
        if self.ws:
            try:
                self.ws.close()
            except Exception:
                pass
            self.ws = None
        self.connected = False
        print("Disconnected from Spider Brain")
    
    def send_command(self, command: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Send a JSON command and return the response (if any)."""
        if not self.connected or not self.ws:
            print("ERROR: Not connected to Brain daemon")
            return None
        
        try:
            msg = json.dumps(command)
            self.ws.send(msg)
            
            # Try to receive response (non-blocking with short timeout)
            self.ws.settimeout(0.5)
            try:
                response = self.ws.recv()
                return json.loads(response) if response else None
            except websocket.WebSocketTimeoutException:
                return None  # No response expected for some commands
            finally:
                self.ws.settimeout(self.timeout)
                
        except websocket.WebSocketConnectionClosedException:
            print("ERROR: Connection closed by server")
            self.connected = False
            return None
        except Exception as e:
            print(f"ERROR: Failed to send command: {e}")
            return None
    
    # ===== Calibration Methods =====
    
    def set_calibration(self, offsets: List[int]) -> None:
        """
        Set calibration offsets in microseconds for all channels.
        
        Args:
            offsets: List of offset values in microseconds. Length must match
                     SERVO_COUNT_TOTAL (13). Values are clamped to ±30° range.
        """
        if len(offsets) != self.SERVO_COUNT_TOTAL:
            raise ValueError(f"Expected {self.SERVO_COUNT_TOTAL} offsets, got {len(offsets)}")
        
        max_offset_us = int(self.CALIB_OFFSET_MAX_DEG * (2000.0 / 180.0))
        self._calibration_offsets_us = [
            max(-max_offset_us, min(max_offset_us, int(o)))
            for o in offsets
        ]
        self._apply_calibration = True
    
    def set_calibration_deg(self, offsets_deg: List[float]) -> None:
        """
        Set calibration offsets in degrees for all channels.
        
        Args:
            offsets_deg: List of offset values in degrees (-30 to +30).
                         Length must match SERVO_COUNT_TOTAL (13).
        """
        if len(offsets_deg) != self.SERVO_COUNT_TOTAL:
            raise ValueError(f"Expected {self.SERVO_COUNT_TOTAL} offsets, got {len(offsets_deg)}")
        
        offsets_us = [int(deg * (2000.0 / 180.0)) for deg in offsets_deg]
        self.set_calibration(offsets_us)
    
    def clear_calibration(self) -> None:
        """Disable calibration (offsets will not be applied)."""
        self._apply_calibration = False
        self._calibration_offsets_us = [0] * self.SERVO_COUNT_TOTAL
    
    def get_calibration(self) -> List[int]:
        """Get current calibration offsets in microseconds."""
        return self._calibration_offsets_us.copy()
    
    def _apply_calib_offset(self, channel: int, us: int, apply_calibration: bool = True) -> int:
        """Apply calibration offset to a PWM value if enabled."""
        if apply_calibration and self._apply_calibration and 0 <= channel < len(self._calibration_offsets_us):
            us += self._calibration_offsets_us[channel]
        return max(self.SERVO_MIN_US, min(self.SERVO_MAX_US, us))
    
    # ===== Servo Control Methods =====
    
    def set_servo(self, channel: int, us: int, apply_calibration: bool = True) -> bool:
        """
        Set a single servo position in microseconds.
        
        Args:
            channel: Servo channel (0-11 for legs)
            us: Pulse width in microseconds (500-2500)
            apply_calibration: If True and calibration is set, apply offset
        """
        if not 0 <= channel < self.SERVO_COUNT:
            print(f"ERROR: Invalid channel {channel}. Must be 0-{self.SERVO_COUNT-1}")
            return False
        
        us = self._apply_calib_offset(channel, us, apply_calibration)
        
        command = {"type": "servo", "channel": channel, "us": us}
        self.send_command(command)
        return True
    
    def set_all_servos(self, us_values: List[int], apply_calibration: bool = True) -> bool:
        """
        Set all servo positions at once.
        
        Args:
            us_values: List of pulse widths (must have SERVO_COUNT values)
            apply_calibration: If True and calibration is set, apply offsets
        """
        if len(us_values) != self.SERVO_COUNT:
            print(f"ERROR: Expected {self.SERVO_COUNT} values, got {len(us_values)}")
            return False
        
        calibrated_values = [
            self._apply_calib_offset(ch, us, apply_calibration)
            for ch, us in enumerate(us_values)
        ]
        
        command = {"type": "servos", "us": calibrated_values}
        self.send_command(command)
        return True
    
    def set_leg(self, leg: int, coxa_us: int, femur_us: int, tibia_us: int, 
                apply_calibration: bool = True) -> bool:
        """Set all three servos of a leg at once."""
        if leg not in self.LEG_CHANNELS:
            print(f"ERROR: Invalid leg {leg}. Must be 0-3")
            return False
        
        channels = self.LEG_CHANNELS[leg]
        self.set_servo(channels[0], coxa_us, apply_calibration)
        self.set_servo(channels[1], femur_us, apply_calibration)
        self.set_servo(channels[2], tibia_us, apply_calibration)
        return True
    
    def all_neutral(self, apply_calibration: bool = True) -> bool:
        """Set all servos to neutral position (1500us)."""
        print("Setting all servos to neutral (1500us)")
        return self.set_all_servos([self.SERVO_NEUTRAL_US] * self.SERVO_COUNT, apply_calibration)
    
    def estop(self) -> bool:
        """Emergency stop - disable all servos immediately."""
        print("!!! E-STOP ACTIVATED !!!")
        command = {"type": "estop"}
        self.send_command(command)
        return True
    
    def get_status(self) -> Optional[Dict[str, Any]]:
        """Query Brain daemon status."""
        command = {"type": "status"}
        return self.send_command(command)
    
    # ===== Eye Control Methods =====
    
    def eye_look(self, x: float, y: float) -> bool:
        """Set eye pupil position. x,y in range [-1.0, 1.0], 0 = center."""
        x = max(-1.0, min(1.0, x))
        y = max(-1.0, min(1.0, y))
        command = {"type": "look", "x": x, "y": y}
        self.send_command(command)
        return True
    
    def eye_blink(self) -> bool:
        """Trigger a blink animation on both eyes."""
        command = {"type": "blink"}
        self.send_command(command)
        return True
    
    def eye_wink(self, eye: str = "left") -> bool:
        """Trigger a wink animation. eye = 'left' or 'right'."""
        command = {"type": "wink", "eye": eye}
        self.send_command(command)
        return True
    
    def eye_mood(self, mood: str) -> bool:
        """Set eye mood. Options: 'normal', 'angry', 'happy', 'sleepy'."""
        if mood not in ("normal", "angry", "happy", "sleepy"):
            print(f"WARNING: Unknown mood '{mood}', using 'normal'")
            mood = "normal"
        command = {"type": "mood", "mood": mood}
        self.send_command(command)
        return True
    
    def eye_idle(self, enabled: bool = True) -> bool:
        """Enable or disable idle eye animation."""
        action = "idle_on" if enabled else "idle_off"
        command = {"type": "eye", "action": action}
        self.send_command(command)
        return True
    
    # ===== Distance Sensor Methods =====
    
    def get_distance(self) -> Optional[int]:
        """
        Read distance from VL53L0X sensor.
        
        Returns:
            Distance in millimeters, or None on error.
        """
        command = {"type": "distance"}
        response = self.send_command(command)
        if response and response.get("status") == "ok":
            return response.get("distance_mm")
        return None
    
    def is_obstacle_close(self, threshold_mm: int = 150) -> bool:
        """Check if an obstacle is within the given threshold."""
        distance = self.get_distance()
        if distance is None:
            return False  # Assume safe if sensor unavailable
        return distance < threshold_mm
    
    # ===== Scan Servo Methods (CH12) =====
    
    def scan_set(self, us: int, apply_calibration: bool = True) -> bool:
        """Set scan servo position in microseconds."""
        us = self._apply_calib_offset(self.SERVO_CHANNEL_SCAN, us, apply_calibration)
        command = {"type": "scan", "us": us}
        self.send_command(command)
        return True
    
    def scan_angle(self, angle_deg: float, apply_calibration: bool = True) -> bool:
        """
        Set scan servo angle in degrees (0-180, center=90).
        
        Maps angle to pulse width:
            0°   -> 500us
            90°  -> 1500us
            180° -> 2500us
        """
        angle_deg = max(0.0, min(180.0, angle_deg))
        us = int(500 + (angle_deg / 180.0) * 2000)
        return self.scan_set(us, apply_calibration)
    
    def scan_center(self, apply_calibration: bool = True) -> bool:
        """Center the scan servo (90°, 1500us)."""
        return self.scan_set(self.SERVO_NEUTRAL_US, apply_calibration)
    
    def scan_left(self, angle: float = 45.0, apply_calibration: bool = True) -> bool:
        """Point scan servo to the left (90 + angle degrees)."""
        return self.scan_angle(90.0 + angle, apply_calibration)
    
    def scan_right(self, angle: float = 45.0, apply_calibration: bool = True) -> bool:
        """Point scan servo to the right (90 - angle degrees)."""
        return self.scan_angle(90.0 - angle, apply_calibration)
    
    def scan_sweep(self, min_angle: float = 0.0, max_angle: float = 180.0, 
                   steps: int = 9, delay_s: float = 0.15,
                   apply_calibration: bool = True) -> List[Optional[int]]:
        """
        Perform a sweep with the scan servo, reading distance at each step.
        
        Args:
            min_angle: Starting angle in degrees (0 = full right)
            max_angle: Ending angle in degrees (180 = full left)
            steps: Number of measurement points
            delay_s: Delay between steps for servo travel
            apply_calibration: If True and calibration is set, apply offset
            
        Returns:
            List of distance readings (mm) at each step, None for failed readings.
        """
        readings = []
        step_size = (max_angle - min_angle) / max(1, steps - 1)
        
        for i in range(steps):
            angle = min_angle + i * step_size
            self.scan_angle(angle, apply_calibration)
            time.sleep(delay_s)
            distance = self.get_distance()
            readings.append(distance)
        
        return readings
    
    def __enter__(self):
        """Context manager entry."""
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.disconnect()
        return False


def main():
    """Test basic connectivity."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Spider Client - Test Connection")
    parser.add_argument("--ip", default="192.168.42.1", help="Brain daemon IP address")
    parser.add_argument("--port", type=int, default=9000, help="Brain daemon port")
    args = parser.parse_args()
    
    client = SpiderClient(host=args.ip, port=args.port)
    
    if client.connect():
        print("\n--- Testing status query ---")
        status = client.get_status()
        if status:
            print(f"Status: {json.dumps(status, indent=2)}")
        else:
            print("No status response (may be normal)")
        
        client.disconnect()
    else:
        print("Failed to connect to Brain daemon")
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())
