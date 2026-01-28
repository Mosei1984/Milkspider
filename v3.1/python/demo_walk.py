#!/usr/bin/env python3
"""
Spider Robot v3.1 - Simple Walking Demo

Demonstrates a basic 4-leg walking gait by cycling through poses.
Uses a tripod gait pattern where diagonal legs move together.

Leg mapping:
  Leg 0 (FR) - Front Right: channels 0,1,2
  Leg 1 (FL) - Front Left:  channels 3,4,5
  Leg 2 (RR) - Rear Right:  channels 6,7,8
  Leg 3 (RL) - Rear Left:   channels 9,10,11

Joint mapping per leg:
  Index 0: Coxa  (hip rotation, horizontal)
  Index 1: Femur (upper leg, vertical)
  Index 2: Tibia (lower leg, vertical)

Usage: python demo_walk.py [--ip 192.168.42.1] [--cycles 3]
"""

import argparse
import time
import sys

from spider_client import SpiderClient


# Servo positions in microseconds
# Neutral is 1500us, range typically 1000-2000us for safe operation

# Standing pose - all legs at neutral
POSE_STAND = [
    # Leg 0 (FR): coxa, femur, tibia
    1500, 1500, 1500,
    # Leg 1 (FL)
    1500, 1500, 1500,
    # Leg 2 (RR)
    1500, 1500, 1500,
    # Leg 3 (RL)
    1500, 1500, 1500,
]

# Tripod Gait - Group A: FR(0) + RL(3), Group B: FL(1) + RR(2)
# Each step has: lift, swing forward, place, push back

# Gait poses for forward walking
GAIT_POSES = [
    # Pose 0: Stand ready
    {
        "name": "Stand",
        "servos": POSE_STAND.copy(),
        "duration": 0.5,
    },
    
    # Pose 1: Lift Group A (FR + RL)
    {
        "name": "Lift A (FR+RL)",
        "servos": [
            # FR - lift leg (femur up, tibia up)
            1500, 1300, 1300,
            # FL - stay planted
            1500, 1500, 1500,
            # RR - stay planted
            1500, 1500, 1500,
            # RL - lift leg
            1500, 1300, 1300,
        ],
        "duration": 0.3,
    },
    
    # Pose 2: Swing Group A forward, push Group B back
    {
        "name": "Swing A fwd",
        "servos": [
            # FR - swing forward (coxa forward)
            1650, 1300, 1300,
            # FL - push back (coxa back)
            1350, 1500, 1500,
            # RR - push back
            1350, 1500, 1500,
            # RL - swing forward
            1650, 1300, 1300,
        ],
        "duration": 0.3,
    },
    
    # Pose 3: Place Group A
    {
        "name": "Place A",
        "servos": [
            # FR - place down
            1650, 1500, 1500,
            # FL - planted
            1350, 1500, 1500,
            # RR - planted
            1350, 1500, 1500,
            # RL - place down
            1650, 1500, 1500,
        ],
        "duration": 0.3,
    },
    
    # Pose 4: Lift Group B (FL + RR)
    {
        "name": "Lift B (FL+RR)",
        "servos": [
            # FR - planted
            1650, 1500, 1500,
            # FL - lift
            1350, 1300, 1300,
            # RR - lift
            1350, 1300, 1300,
            # RL - planted
            1650, 1500, 1500,
        ],
        "duration": 0.3,
    },
    
    # Pose 5: Swing Group B forward, push Group A back
    {
        "name": "Swing B fwd",
        "servos": [
            # FR - push back to center
            1500, 1500, 1500,
            # FL - swing forward
            1650, 1300, 1300,
            # RR - swing forward
            1650, 1300, 1300,
            # RL - push back to center
            1500, 1500, 1500,
        ],
        "duration": 0.3,
    },
    
    # Pose 6: Place Group B (returns to near-stand position)
    {
        "name": "Place B",
        "servos": [
            # FR - neutral
            1500, 1500, 1500,
            # FL - place down forward
            1650, 1500, 1500,
            # RR - place down forward
            1650, 1500, 1500,
            # RL - neutral
            1500, 1500, 1500,
        ],
        "duration": 0.3,
    },
    
    # The gait then continues from Pose 1 with legs in swapped positions
    # For simplicity, we reset to stand and repeat
]


def safe_startup(client: SpiderClient) -> bool:
    """Safely initialize robot to standing position."""
    print("\n--- Safe Startup Sequence ---")
    print("Setting all servos to neutral...")
    
    if not client.all_neutral():
        print("ERROR: Failed to set neutral position")
        return False
    
    time.sleep(1.0)  # Give servos time to reach position
    print("Robot is now standing at neutral position")
    return True


def execute_pose(client: SpiderClient, pose: dict, verbose: bool = True):
    """Execute a single pose."""
    if verbose:
        print(f"  -> {pose['name']}", end="", flush=True)
    
    client.set_all_servos(pose["servos"])
    time.sleep(pose["duration"])
    
    if verbose:
        print(" ✓")


def walk_cycle(client: SpiderClient, verbose: bool = True):
    """Execute one complete walking cycle."""
    for pose in GAIT_POSES[1:]:  # Skip stand pose
        execute_pose(client, pose, verbose)


def demo_walk(client: SpiderClient, cycles: int = 3):
    """Run the walking demo for specified number of cycles."""
    print(f"\n{'='*50}")
    print("SPIDER ROBOT v3.1 - WALKING DEMO")
    print(f"{'='*50}")
    print(f"Cycles: {cycles}")
    print("Press Ctrl+C to stop safely\n")
    
    # Safe startup
    if not safe_startup(client):
        return False
    
    input("Press Enter to start walking demo...")
    
    try:
        for cycle in range(cycles):
            print(f"\n--- Cycle {cycle + 1}/{cycles} ---")
            walk_cycle(client)
        
        print("\n--- Walk complete, returning to stand ---")
        execute_pose(client, GAIT_POSES[0])  # Return to stand
        
    except KeyboardInterrupt:
        print("\n\n!!! INTERRUPTED !!!")
        print("Returning to safe position...")
        client.all_neutral()
        time.sleep(0.5)
        return False
    
    return True


def demo_leg_test(client: SpiderClient):
    """Test each leg individually for debugging."""
    print(f"\n{'='*50}")
    print("LEG RANGE OF MOTION TEST")
    print(f"{'='*50}")
    
    if not safe_startup(client):
        return
    
    for leg in range(4):
        leg_name = client.LEG_NAMES[leg]
        channels = client.LEG_CHANNELS[leg]
        
        print(f"\n--- Testing Leg {leg} ({leg_name}) ---")
        input(f"Press Enter to test {leg_name}...")
        
        # Test coxa (horizontal swing)
        print("  Coxa sweep...")
        client.set_servo(channels[0], 1300)
        time.sleep(0.5)
        client.set_servo(channels[0], 1700)
        time.sleep(0.5)
        client.set_servo(channels[0], 1500)
        time.sleep(0.3)
        
        # Test femur (upper leg)
        print("  Femur sweep...")
        client.set_servo(channels[1], 1300)
        time.sleep(0.5)
        client.set_servo(channels[1], 1700)
        time.sleep(0.5)
        client.set_servo(channels[1], 1500)
        time.sleep(0.3)
        
        # Test tibia (lower leg)
        print("  Tibia sweep...")
        client.set_servo(channels[2], 1300)
        time.sleep(0.5)
        client.set_servo(channels[2], 1700)
        time.sleep(0.5)
        client.set_servo(channels[2], 1500)
        time.sleep(0.3)
    
    print("\nLeg test complete!")


def main():
    parser = argparse.ArgumentParser(
        description="Spider Robot v3.1 - Walking Demo"
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
        "--cycles",
        type=int,
        default=3,
        help="Number of walk cycles (default: 3)"
    )
    parser.add_argument(
        "--leg-test",
        action="store_true",
        help="Run leg range-of-motion test instead of walk"
    )
    args = parser.parse_args()
    
    print("Spider Robot v3.1 - Walking Demo")
    print("================================")
    print(f"Target: {args.ip}:{args.port}")
    print()
    
    client = SpiderClient(host=args.ip, port=args.port)
    
    if not client.connect():
        print("\nConnection failed!")
        return 1
    
    try:
        if args.leg_test:
            demo_leg_test(client)
        else:
            demo_walk(client, cycles=args.cycles)
        
        print("\nDemo complete - sending safe neutral position")
        client.all_neutral()
        
    except KeyboardInterrupt:
        print("\n\n!!! E-STOP - Keyboard Interrupt !!!")
        client.estop()
    except Exception as e:
        print(f"\n!!! ERROR: {e} !!!")
        print("Sending E-STOP for safety")
        client.estop()
        return 1
    finally:
        client.disconnect()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
