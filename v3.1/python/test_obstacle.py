#!/usr/bin/env python3
"""
Spider Robot v3.1 - Obstacle Avoidance Test

Interactive test for the obstacle avoidance module.
Shows scan visualization and recommended actions.

Usage:
    python test_obstacle.py                    # Connect to real hardware
    python test_obstacle.py --simulate         # Simulation mode (no hardware)
    python test_obstacle.py --interactive      # Interactive mode with commands
    python test_obstacle.py --continuous       # Continuous scanning mode
"""

import argparse
import sys
import time
import logging

from obstacle_avoidance import ObstacleAvoidance, Action, ScanData, create_obstacle_avoider

try:
    from spider_client import SpiderClient
except ImportError:
    SpiderClient = None


def print_action_recommendation(action: Action, distance: int = None):
    """Print action with visual indicator."""
    symbols = {
        Action.FORWARD: "^^ FORWARD",
        Action.SLOW_FORWARD: "^  SLOW FORWARD",
        Action.TURN_LEFT: "<< TURN LEFT",
        Action.TURN_RIGHT: ">> TURN RIGHT",
        Action.BACKUP: "vv BACKUP",
        Action.STOP: "## STOP",
    }
    
    symbol = symbols.get(action, str(action))
    dist_str = f" (front: {distance}mm)" if distance else ""
    print(f"\nRecommended: {symbol}{dist_str}")


def print_scan_visualization(scan: ScanData, oa: ObstacleAvoidance):
    """Print ASCII scan visualization with polar-ish display."""
    if not scan or not scan.angles:
        print("No scan data")
        return
    
    print("\n" + "=" * 50)
    print("SCAN RESULTS")
    print("=" * 50)
    
    # Text-based visualization
    print(oa.format_scan_ascii(scan))
    
    # Simple top-down view
    print("\nTop-Down View (Robot at bottom):")
    print("         Left  Center  Right")
    
    max_dist = scan.max_distance or 1000
    
    # Build a simple grid
    width = 40
    height = 10
    grid = [[' ' for _ in range(width)] for _ in range(height)]
    
    # Mark robot position
    robot_x = width // 2
    robot_y = height - 1
    grid[robot_y][robot_x] = 'R'
    
    # Mark obstacles based on scan
    for angle, dist in zip(scan.angles, scan.distances):
        if dist is None:
            continue
        
        # Convert polar to cartesian-ish
        import math
        rad = math.radians(angle)
        # Scale distance to grid
        scaled_dist = min(height - 2, int((dist / max_dist) * (height - 2)))
        
        # X position: angle 30° = right edge, 150° = left edge
        x_ratio = (angle - 30) / 120  # 0 at 30°, 1 at 150°
        x = int(x_ratio * (width - 2)) + 1
        y = robot_y - scaled_dist - 1
        
        if 0 <= x < width and 0 <= y < height:
            if dist <= oa.critical_distance:
                grid[y][x] = '!'
            elif dist <= oa.warning_distance:
                grid[y][x] = '*'
            else:
                grid[y][x] = '.'
    
    # Print grid
    print("  +" + "-" * width + "+")
    for row in grid:
        print("  |" + "".join(row) + "|")
    print("  +" + "-" * width + "+")
    print("  Legend: R=Robot  !=Critical  *=Warning  .=Clear")


def run_single_test(oa: ObstacleAvoidance):
    """Run a single scan and show results."""
    print("\nPerforming environment scan...")
    scan = oa.scan_environment()
    
    print_scan_visualization(scan, oa)
    
    action = oa.get_recommended_action(use_cached_scan=True)
    _, front_dist = oa.check_front()
    print_action_recommendation(action, front_dist)


def run_continuous_mode(oa: ObstacleAvoidance, interval: float = 1.0):
    """Continuously scan and display results."""
    print("\nContinuous scanning mode (Ctrl+C to stop)")
    print(f"Scan interval: {interval}s")
    
    try:
        while True:
            # Clear screen (works on most terminals)
            print("\033[2J\033[H", end="")
            
            scan = oa.scan_environment()
            print_scan_visualization(scan, oa)
            
            action = oa.get_recommended_action(use_cached_scan=True)
            _, front_dist = oa.check_front()
            print_action_recommendation(action, front_dist)
            
            print(f"\nNext scan in {interval}s... (Ctrl+C to stop)")
            time.sleep(interval)
            
    except KeyboardInterrupt:
        print("\nStopped continuous mode")


def run_interactive_mode(oa: ObstacleAvoidance, client):
    """Interactive testing mode with commands."""
    print("\nInteractive Obstacle Avoidance Test")
    print("=" * 40)
    print("Commands:")
    print("  s, scan     - Full environment scan")
    print("  f, front    - Quick front check")
    print("  a, action   - Get recommended action")
    print("  l, left     - Scan left side")
    print("  r, right    - Scan right side")
    print("  c, center   - Center scan servo")
    print("  e, eyes     - Toggle eye integration")
    print("  t, thresholds - Show/set distance thresholds")
    print("  q, quit     - Exit")
    print()
    
    eyes_enabled = False
    
    while True:
        try:
            cmd = input("obstacle> ").strip().lower()
        except EOFError:
            break
        except KeyboardInterrupt:
            print()
            break
        
        if not cmd:
            continue
        
        if cmd in ('q', 'quit', 'exit'):
            break
        
        elif cmd in ('s', 'scan'):
            run_single_test(oa)
        
        elif cmd in ('f', 'front'):
            is_safe, dist = oa.check_front()
            status = "SAFE" if is_safe else "OBSTACLE"
            print(f"Front check: {dist}mm - {status}")
        
        elif cmd in ('a', 'action'):
            action = oa.get_recommended_action()
            _, front_dist = oa.check_front()
            print_action_recommendation(action, front_dist)
        
        elif cmd in ('l', 'left'):
            if client and not oa._simulate:
                client.scan_left()
                time.sleep(0.15)
                dist = oa._get_distance()
                print(f"Left (135°): {dist}mm")
        
        elif cmd in ('r', 'right'):
            if client and not oa._simulate:
                client.scan_right()
                time.sleep(0.15)
                dist = oa._get_distance()
                print(f"Right (45°): {dist}mm")
        
        elif cmd in ('c', 'center'):
            if client and not oa._simulate:
                client.scan_center()
                time.sleep(0.15)
                dist = oa._get_distance()
                print(f"Center (90°): {dist}mm")
        
        elif cmd in ('e', 'eyes'):
            if client:
                eyes_enabled = not eyes_enabled
                if eyes_enabled:
                    oa.integrate_with_eyes(client)
                    print("Eye integration: ENABLED")
                else:
                    oa.set_eye_callbacks(None, None, None)
                    print("Eye integration: DISABLED")
            else:
                print("No client connected")
        
        elif cmd in ('t', 'thresholds'):
            print(f"Current thresholds (mm):")
            print(f"  Critical: {oa.critical_distance}")
            print(f"  Warning:  {oa.warning_distance}")
            print(f"  Safe:     {oa.safe_distance}")
            
            try:
                new_val = input("Set new values? (critical warning safe, or Enter to skip): ").strip()
                if new_val:
                    parts = new_val.split()
                    if len(parts) == 3:
                        oa.critical_distance = int(parts[0])
                        oa.warning_distance = int(parts[1])
                        oa.safe_distance = int(parts[2])
                        print("Thresholds updated")
            except ValueError:
                print("Invalid input")
        
        else:
            print(f"Unknown command: {cmd}")


def main():
    parser = argparse.ArgumentParser(
        description="Spider Robot v3.1 - Obstacle Avoidance Test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python test_obstacle.py --simulate           # Test without hardware
  python test_obstacle.py --interactive        # Interactive command mode
  python test_obstacle.py --continuous --rate 0.5  # Fast continuous scanning
        """
    )
    parser.add_argument("--ip", default="192.168.42.1",
                        help="Spider Brain IP address")
    parser.add_argument("--port", type=int, default=9000,
                        help="Spider Brain port")
    parser.add_argument("--simulate", "-s", action="store_true",
                        help="Simulation mode (no hardware required)")
    parser.add_argument("--interactive", "-i", action="store_true",
                        help="Interactive command mode")
    parser.add_argument("--continuous", "-c", action="store_true",
                        help="Continuous scanning mode")
    parser.add_argument("--rate", type=float, default=1.0,
                        help="Scan interval for continuous mode (seconds)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Enable debug logging")
    args = parser.parse_args()
    
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG, format="%(levelname)s: %(message)s")
    else:
        logging.basicConfig(level=logging.INFO, format="%(message)s")
    
    print("Spider Robot v3.1 - Obstacle Avoidance Test")
    print("=" * 45)
    print(f"Target: {args.ip}:{args.port}")
    print(f"Mode: {'Simulation' if args.simulate else 'Hardware'}")
    print()
    
    client = None
    oa = None
    
    try:
        if args.simulate:
            # Create client but don't require connection
            if SpiderClient:
                client = SpiderClient(host=args.ip, port=args.port)
            oa = ObstacleAvoidance(client)
            oa.set_simulate(True)
            print("Running in SIMULATION mode (fake sensor data)")
        else:
            if SpiderClient is None:
                print("ERROR: spider_client module not found")
                return 1
            
            client = SpiderClient(host=args.ip, port=args.port)
            if not client.connect():
                print("ERROR: Failed to connect to Spider Brain")
                print("Use --simulate flag to test without hardware")
                return 1
            
            oa = ObstacleAvoidance(client)
            print("Connected to Spider Brain")
        
        # Run selected mode
        if args.interactive:
            run_interactive_mode(oa, client)
        elif args.continuous:
            run_continuous_mode(oa, args.rate)
        else:
            # Single test
            run_single_test(oa)
        
    except KeyboardInterrupt:
        print("\nInterrupted")
    
    finally:
        if client and not args.simulate:
            try:
                client.scan_center()
                time.sleep(0.2)
                client.disconnect()
            except Exception:
                pass
    
    print("\nTest complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
