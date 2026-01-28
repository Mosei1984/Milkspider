#!/usr/bin/env python3
"""
Spider Robot v3.1 - Scan Servo Test Script

Tests the scan servo (CH12) used for sweeping the distance sensor.
"""

import argparse
import time
import sys
from spider_client import SpiderClient


def test_basic_motion(client: SpiderClient):
    """Test basic scan servo positions."""
    print("\n=== Basic Motion Test ===")
    
    print("Center (90°)...")
    client.scan_center()
    time.sleep(0.5)
    
    print("Left (135°)...")
    client.scan_left(45)
    time.sleep(0.5)
    
    print("Right (45°)...")
    client.scan_right(45)
    time.sleep(0.5)
    
    print("Full left (180°)...")
    client.scan_angle(180)
    time.sleep(0.5)
    
    print("Full right (0°)...")
    client.scan_angle(0)
    time.sleep(0.5)
    
    print("Center...")
    client.scan_center()
    time.sleep(0.3)
    
    print("Basic motion test complete!")


def test_sweep(client: SpiderClient):
    """Test sweep with distance readings."""
    print("\n=== Sweep Test ===")
    print("Performing 9-point sweep (0° to 180°)...")
    
    readings = client.scan_sweep(min_angle=0, max_angle=180, steps=9, delay_s=0.2)
    
    print("\nResults:")
    print("-" * 40)
    for i, dist in enumerate(readings):
        angle = i * (180 / 8)
        if dist is not None:
            bar = "#" * min(50, dist // 20)
            print(f"  {angle:5.1f}°: {dist:4d}mm {bar}")
        else:
            print(f"  {angle:5.1f}°: --error--")
    print("-" * 40)
    
    # Return to center
    print("Returning to center...")
    client.scan_center()
    time.sleep(0.3)
    
    print("Sweep test complete!")


def test_continuous_scan(client: SpiderClient, duration: float = 5.0):
    """Continuous back-and-forth scanning."""
    print(f"\n=== Continuous Scan Test ({duration}s) ===")
    
    start = time.time()
    angle = 90.0
    direction = 1
    speed = 90.0  # degrees per second
    
    while (time.time() - start) < duration:
        client.scan_angle(angle)
        
        # Read distance at current position
        dist = client.get_distance()
        if dist is not None:
            print(f"  {angle:5.1f}°: {dist:4d}mm", end="\r")
        
        time.sleep(0.05)
        angle += direction * speed * 0.05
        
        if angle >= 160:
            direction = -1
        elif angle <= 20:
            direction = 1
    
    print("\nContinuous scan complete!")
    client.scan_center()


def test_interactive(client: SpiderClient):
    """Interactive test mode."""
    print("\n=== Interactive Mode ===")
    print("Commands:")
    print("  c     - Center")
    print("  l     - Left 45°")
    print("  r     - Right 45°")
    print("  L     - Full left (180°)")
    print("  R     - Full right (0°)")
    print("  s     - Sweep scan")
    print("  d     - Read distance")
    print("  0-180 - Set specific angle")
    print("  q     - Quit")
    print()
    
    while True:
        try:
            cmd = input("scan> ").strip()
        except (KeyboardInterrupt, EOFError):
            print()
            break
        
        if not cmd:
            continue
        
        if cmd == 'q':
            break
        elif cmd == 'c':
            client.scan_center()
        elif cmd == 'l':
            client.scan_left(45)
        elif cmd == 'r':
            client.scan_right(45)
        elif cmd == 'L':
            client.scan_angle(180)
        elif cmd == 'R':
            client.scan_angle(0)
        elif cmd == 's':
            test_sweep(client)
        elif cmd == 'd':
            dist = client.get_distance()
            if dist is not None:
                print(f"  Distance: {dist}mm")
            else:
                print("  Distance: --error--")
        else:
            try:
                angle = float(cmd)
                if 0 <= angle <= 180:
                    client.scan_angle(angle)
                    print(f"  -> {angle}°")
                else:
                    print("  Angle must be 0-180")
            except ValueError:
                print(f"  Unknown command: {cmd}")


def main():
    parser = argparse.ArgumentParser(description="Spider Scan Servo Test")
    parser.add_argument("--ip", default="192.168.42.1", help="Brain daemon IP")
    parser.add_argument("--port", type=int, default=9000, help="Brain daemon port")
    parser.add_argument("--test", choices=["basic", "sweep", "continuous", "interactive", "all"],
                        default="interactive", help="Test to run")
    args = parser.parse_args()
    
    client = SpiderClient(host=args.ip, port=args.port)
    
    if not client.connect():
        print("Failed to connect to Brain daemon")
        return 1
    
    try:
        if args.test == "basic" or args.test == "all":
            test_basic_motion(client)
        
        if args.test == "sweep" or args.test == "all":
            test_sweep(client)
        
        if args.test == "continuous":
            test_continuous_scan(client)
        
        if args.test == "interactive":
            test_interactive(client)
        
        if args.test == "all":
            print("\n=== All Tests Complete ===")
    
    except KeyboardInterrupt:
        print("\nInterrupted!")
    finally:
        client.scan_center()
        client.disconnect()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
