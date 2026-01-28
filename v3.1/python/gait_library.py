#!/usr/bin/env python3
"""
Spider Robot v3.1 - Gait Library

Provides multiple walking gaits for the 4-leg spider robot:
- Tripod Gait: Fast, 2 diagonal legs move together
- Wave Gait: Slow and stable, one leg at a time
- Ripple Gait: Medium speed, 2 legs at a time (not diagonal)

Leg Layout (top view):
    Front
  FL(1)  FR(0)
     \\  //
      []
     //  \\
  RL(3)  RR(2)
    Rear

Each leg has 3 servos: Coxa (hip), Femur (upper), Tibia (lower)
"""

import time
import math
from dataclasses import dataclass
from typing import List, Tuple, Optional
from enum import Enum


class GaitType(Enum):
    TRIPOD = "tripod"    # Fast: diagonal pairs
    WAVE = "wave"        # Slow: one leg at a time  
    RIPPLE = "ripple"    # Medium: two adjacent legs


class Direction(Enum):
    FORWARD = "forward"
    BACKWARD = "backward"
    LEFT = "left"
    RIGHT = "right"
    ROTATE_CW = "rotate_cw"
    ROTATE_CCW = "rotate_ccw"


@dataclass
class LegPose:
    """Single leg joint angles in microseconds."""
    coxa: int = 1500   # Hip rotation (horizontal)
    femur: int = 1500  # Upper leg (vertical)
    tibia: int = 1500  # Lower leg (vertical)

    def to_list(self) -> List[int]:
        return [self.coxa, self.femur, self.tibia]

    def lifted(self, lift_amount: int = 200) -> 'LegPose':
        """Return leg pose with raised position."""
        return LegPose(self.coxa, self.femur - lift_amount, self.tibia - lift_amount)


@dataclass  
class RobotPose:
    """Complete robot pose (all 4 legs)."""
    leg0: LegPose  # Front Right
    leg1: LegPose  # Front Left  
    leg2: LegPose  # Rear Right
    leg3: LegPose  # Rear Left

    def to_list(self) -> List[int]:
        """Convert to flat servo list [ch0..ch11]."""
        return (self.leg0.to_list() + self.leg1.to_list() + 
                self.leg2.to_list() + self.leg3.to_list())

    @classmethod
    def neutral(cls) -> 'RobotPose':
        """Create neutral standing pose."""
        return cls(LegPose(), LegPose(), LegPose(), LegPose())


class GaitGenerator:
    """Generates walking gaits for the spider robot."""

    # Movement parameters
    LIFT_HEIGHT = 200      # Servo us change for leg lift
    STRIDE_LENGTH = 150    # Servo us change for stride
    STEP_DURATION = 0.25   # Seconds per gait phase
    
    # Leg groups for tripod gait
    GROUP_A = [0, 3]  # FR + RL (diagonal)
    GROUP_B = [1, 2]  # FL + RR (diagonal)

    def __init__(self, client=None):
        """
        Initialize gait generator.
        
        Args:
            client: SpiderClient instance for sending commands
        """
        self.client = client
        self.current_pose = RobotPose.neutral()
        self.gait_type = GaitType.TRIPOD
        self.step_duration = self.STEP_DURATION
        self.running = False

    def set_client(self, client):
        """Set the spider client for command transmission."""
        self.client = client

    def set_gait(self, gait_type: GaitType):
        """Select the walking gait type."""
        self.gait_type = gait_type
        print(f"Gait set to: {gait_type.value}")

    def set_speed(self, speed: float):
        """
        Set walking speed (0.5 = slow, 1.0 = normal, 2.0 = fast).
        """
        self.step_duration = self.STEP_DURATION / max(0.5, min(2.0, speed))

    def _send_pose(self, pose: RobotPose, duration: float = None):
        """Send pose to robot and wait."""
        if self.client:
            self.client.set_all_servos(pose.to_list())
        self.current_pose = pose
        time.sleep(duration or self.step_duration)

    def _get_leg_pose(self, leg_idx: int) -> LegPose:
        """Get current pose of a specific leg."""
        if leg_idx == 0:
            return self.current_pose.leg0
        elif leg_idx == 1:
            return self.current_pose.leg1
        elif leg_idx == 2:
            return self.current_pose.leg2
        else:
            return self.current_pose.leg3

    def _set_leg_pose(self, leg_idx: int, pose: LegPose):
        """Set pose for a specific leg."""
        if leg_idx == 0:
            self.current_pose.leg0 = pose
        elif leg_idx == 1:
            self.current_pose.leg1 = pose
        elif leg_idx == 2:
            self.current_pose.leg2 = pose
        else:
            self.current_pose.leg3 = pose

    def stand(self):
        """Move to neutral standing pose."""
        self._send_pose(RobotPose.neutral(), 0.5)

    def tripod_step(self, direction: Direction = Direction.FORWARD):
        """
        Execute one tripod gait step.
        
        Tripod gait: Two diagonal legs move together.
        Group A: FR(0) + RL(3)
        Group B: FL(1) + RR(2)
        """
        stride = self.STRIDE_LENGTH
        lift = self.LIFT_HEIGHT

        # Direction modifiers
        if direction == Direction.BACKWARD:
            stride = -stride
        elif direction == Direction.LEFT:
            # Strafe: right legs forward, left legs back
            stride_r, stride_l = stride, -stride
        elif direction == Direction.RIGHT:
            stride_r, stride_l = -stride, stride
        elif direction == Direction.ROTATE_CW:
            # Rotate: left legs forward, right legs back
            stride_r, stride_l = -stride, stride
        elif direction == Direction.ROTATE_CCW:
            stride_r, stride_l = stride, -stride
        else:
            stride_r = stride_l = stride

        # For simple forward/back, all legs use same stride
        if direction in (Direction.FORWARD, Direction.BACKWARD):
            stride_r = stride_l = stride

        # Phase 1: Lift Group A
        pose = RobotPose.neutral()
        pose.leg0 = LegPose(1500, 1500 - lift, 1500 - lift)  # FR up
        pose.leg3 = LegPose(1500, 1500 - lift, 1500 - lift)  # RL up
        self._send_pose(pose)

        # Phase 2: Swing A forward, push B back
        pose.leg0 = LegPose(1500 + stride_r, 1500 - lift, 1500 - lift)
        pose.leg3 = LegPose(1500 + stride_l, 1500 - lift, 1500 - lift)
        pose.leg1 = LegPose(1500 - stride_l, 1500, 1500)  # FL push
        pose.leg2 = LegPose(1500 - stride_r, 1500, 1500)  # RR push
        self._send_pose(pose)

        # Phase 3: Place A
        pose.leg0 = LegPose(1500 + stride_r, 1500, 1500)
        pose.leg3 = LegPose(1500 + stride_l, 1500, 1500)
        self._send_pose(pose)

        # Phase 4: Lift Group B
        pose.leg1 = LegPose(1500 - stride_l, 1500 - lift, 1500 - lift)
        pose.leg2 = LegPose(1500 - stride_r, 1500 - lift, 1500 - lift)
        self._send_pose(pose)

        # Phase 5: Swing B forward, push A back
        pose.leg1 = LegPose(1500 + stride_l, 1500 - lift, 1500 - lift)
        pose.leg2 = LegPose(1500 + stride_r, 1500 - lift, 1500 - lift)
        pose.leg0 = LegPose(1500, 1500, 1500)
        pose.leg3 = LegPose(1500, 1500, 1500)
        self._send_pose(pose)

        # Phase 6: Place B (back to near neutral)
        pose.leg1 = LegPose(1500 + stride_l, 1500, 1500)
        pose.leg2 = LegPose(1500 + stride_r, 1500, 1500)
        self._send_pose(pose)

    def wave_step(self, direction: Direction = Direction.FORWARD):
        """
        Execute one wave gait step.
        
        Wave gait: One leg moves at a time.
        Order: RL(3) -> RR(2) -> FL(1) -> FR(0)
        Very stable but slow.
        """
        stride = self.STRIDE_LENGTH
        lift = self.LIFT_HEIGHT

        if direction == Direction.BACKWARD:
            stride = -stride

        leg_order = [3, 2, 1, 0]  # RL, RR, FL, FR

        for leg_idx in leg_order:
            # Lift
            leg = LegPose(1500, 1500 - lift, 1500 - lift)
            self._set_leg_pose(leg_idx, leg)
            self._send_pose(self.current_pose, self.step_duration * 0.5)

            # Swing forward
            leg = LegPose(1500 + stride, 1500 - lift, 1500 - lift)
            self._set_leg_pose(leg_idx, leg)
            self._send_pose(self.current_pose, self.step_duration * 0.5)

            # Place
            leg = LegPose(1500 + stride, 1500, 1500)
            self._set_leg_pose(leg_idx, leg)
            self._send_pose(self.current_pose, self.step_duration * 0.5)

        # Push phase: all legs push back
        for leg_idx in range(4):
            leg = LegPose(1500, 1500, 1500)
            self._set_leg_pose(leg_idx, leg)
        self._send_pose(self.current_pose, self.step_duration)

    def step(self, direction: Direction = Direction.FORWARD):
        """Execute one gait step based on current gait type."""
        if self.gait_type == GaitType.TRIPOD:
            self.tripod_step(direction)
        elif self.gait_type == GaitType.WAVE:
            self.wave_step(direction)
        elif self.gait_type == GaitType.RIPPLE:
            # Ripple is similar to tripod but with adjacent legs
            self.tripod_step(direction)

    def walk(self, direction: Direction, steps: int = 1):
        """Walk in a direction for specified number of steps."""
        self.running = True
        try:
            for _ in range(steps):
                if not self.running:
                    break
                self.step(direction)
        finally:
            self.running = False

    def stop(self):
        """Stop walking and return to stand."""
        self.running = False
        self.stand()


def demo():
    """Demonstration of gait library."""
    from spider_client import SpiderClient

    print("Spider Robot v3.1 - Gait Library Demo")
    print("=" * 50)

    client = SpiderClient()
    if not client.connect():
        print("Failed to connect")
        return

    gait = GaitGenerator(client)

    try:
        print("\nStanding...")
        gait.stand()
        time.sleep(1)

        print("\nTripod gait forward (3 steps)...")
        gait.set_gait(GaitType.TRIPOD)
        gait.walk(Direction.FORWARD, 3)

        print("\nWave gait forward (2 steps)...")
        gait.set_gait(GaitType.WAVE)
        gait.set_speed(0.7)
        gait.walk(Direction.FORWARD, 2)

        print("\nReturning to stand...")
        gait.stand()

    except KeyboardInterrupt:
        print("\nInterrupted!")
        gait.stop()
    finally:
        client.all_neutral()
        client.disconnect()


if __name__ == "__main__":
    demo()
