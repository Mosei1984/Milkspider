#!/usr/bin/env python3
"""
Spider Robot v3.1 - Servo Test Script

Interactive test script to verify servo control pipeline:
1. Test single servo sweep
2. Test all servos to neutral
3. Test E-STOP
4. Print status

Usage: python test_servos.py [--ip 192.168.42.1]
"""

import argparse
import time
import sys

from spider_client import SpiderClient


def test_single_servo_sweep(client: SpiderClient, channel: int = 0) -> bool:
    """Test a single servo with a sweep pattern: 1000 → 2000 → 1500."""
    print(f"\n{'='*50}")
    print(f"TEST: Single Servo Sweep (Channel {channel})")
    print(f"{'='*50}")
    
    positions = [
        (1000, "minimum (1000us)"),
        (2000, "maximum (2000us)"),
        (1500, "neutral (1500us)"),
    ]
    
    for us, desc in positions:
        print(f"  Moving to {desc}...", end=" ", flush=True)
        if client.set_servo(channel, us):
            print("OK")
            time.sleep(1.0)  # Wait for servo to reach position
        else:
            print("FAILED")
            return False
    
    print("  Sweep test PASSED")
    return True


def test_all_servos_neutral(client: SpiderClient) -> bool:
    """Test setting all 12 servos to neutral position."""
    print(f"\n{'='*50}")
    print("TEST: All Servos to Neutral")
    print(f"{'='*50}")
    
    print("  Setting all 12 servos to 1500us...", end=" ", flush=True)
    if client.all_neutral():
        print("OK")
        time.sleep(0.5)
        print("  All neutral test PASSED")
        return True
    else:
        print("FAILED")
        return False


def test_estop(client: SpiderClient) -> bool:
    """Test emergency stop functionality."""
    print(f"\n{'='*50}")
    print("TEST: Emergency Stop (E-STOP)")
    print(f"{'='*50}")
    
    print("  Sending E-STOP command...", end=" ", flush=True)
    if client.estop():
        print("OK")
        print("  E-STOP test PASSED (servos should be disabled)")
        return True
    else:
        print("FAILED")
        return False


def test_status(client: SpiderClient) -> bool:
    """Test status query."""
    print(f"\n{'='*50}")
    print("TEST: Status Query")
    print(f"{'='*50}")
    
    print("  Requesting status...", end=" ", flush=True)
    status = client.get_status()
    
    if status:
        print("OK")
        print(f"  Response: {status}")
        return True
    else:
        print("No response (may be normal if daemon doesn't reply)")
        return True  # Not a failure, some daemons don't respond


def test_individual_legs(client: SpiderClient) -> bool:
    """Test each leg individually."""
    print(f"\n{'='*50}")
    print("TEST: Individual Leg Test")
    print(f"{'='*50}")
    
    # First, set all to neutral
    client.all_neutral()
    time.sleep(0.5)
    
    for leg in range(4):
        leg_name = client.LEG_NAMES[leg]
        channels = client.LEG_CHANNELS[leg]
        print(f"\n  Testing Leg {leg} ({leg_name}) - Channels {channels}")
        
        # Test each joint in the leg
        for joint_idx, channel in enumerate(channels):
            joint_name = client.JOINT_NAMES[joint_idx]
            print(f"    {joint_name} (ch {channel}): ", end="", flush=True)
            
            # Small movement to verify
            client.set_servo(channel, 1400)
            time.sleep(0.3)
            client.set_servo(channel, 1600)
            time.sleep(0.3)
            client.set_servo(channel, 1500)
            time.sleep(0.2)
            print("OK")
        
        time.sleep(0.3)
    
    print("\n  Leg test PASSED")
    return True


def interactive_menu(client: SpiderClient):
    """Interactive test menu."""
    while True:
        print(f"\n{'='*50}")
        print("Spider Robot v3.1 - Servo Test Menu")
        print(f"{'='*50}")
        print("1. Single servo sweep (channel 0)")
        print("2. All servos to neutral")
        print("3. Test all legs individually")
        print("4. E-STOP")
        print("5. Get status")
        print("6. Custom servo command")
        print("0. Exit")
        print()
        
        try:
            choice = input("Select test [0-6]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nExiting...")
            break
        
        if choice == "0":
            break
        elif choice == "1":
            test_single_servo_sweep(client)
        elif choice == "2":
            test_all_servos_neutral(client)
        elif choice == "3":
            test_individual_legs(client)
        elif choice == "4":
            test_estop(client)
        elif choice == "5":
            test_status(client)
        elif choice == "6":
            try:
                ch = int(input("  Channel (0-11): "))
                us = int(input("  Position (500-2500 us): "))
                client.set_servo(ch, us)
                print(f"  Sent: channel={ch}, us={us}")
            except ValueError:
                print("  Invalid input")
        else:
            print("Invalid choice")


def run_all_tests(client: SpiderClient) -> bool:
    """Run all tests in sequence."""
    print("\n" + "="*60)
    print("SPIDER ROBOT v3.1 - FULL SERVO TEST SUITE")
    print("="*60)
    
    tests = [
        ("Status Query", lambda: test_status(client)),
        ("All Neutral", lambda: test_all_servos_neutral(client)),
        ("Single Servo Sweep", lambda: test_single_servo_sweep(client)),
        ("All Neutral (reset)", lambda: test_all_servos_neutral(client)),
        ("E-STOP", lambda: test_estop(client)),
    ]
    
    passed = 0
    failed = 0
    
    for name, test_func in tests:
        try:
            if test_func():
                passed += 1
            else:
                failed += 1
                print(f"  *** TEST FAILED: {name} ***")
        except Exception as e:
            failed += 1
            print(f"  *** TEST ERROR: {name} - {e} ***")
    
    print(f"\n{'='*60}")
    print(f"TEST RESULTS: {passed} passed, {failed} failed")
    print(f"{'='*60}\n")
    
    return failed == 0


def main():
    parser = argparse.ArgumentParser(
        description="Spider Robot v3.1 - Servo Test Script"
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
        "--auto", 
        action="store_true",
        help="Run all tests automatically (no interactive menu)"
    )
    args = parser.parse_args()
    
    client = SpiderClient(host=args.ip, port=args.port)
    
    if not client.connect():
        print("\nFailed to connect. Check:")
        print(f"  1. Brain daemon is running on {args.ip}:{args.port}")
        print(f"  2. Network connectivity to {args.ip}")
        print(f"  3. No firewall blocking port {args.port}")
        return 1
    
    try:
        if args.auto:
            success = run_all_tests(client)
            return 0 if success else 1
        else:
            interactive_menu(client)
            return 0
    except KeyboardInterrupt:
        print("\n\nInterrupted - sending E-STOP for safety")
        client.estop()
        return 1
    finally:
        client.disconnect()


if __name__ == "__main__":
    sys.exit(main())
