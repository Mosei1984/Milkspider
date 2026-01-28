#!/usr/bin/env python3
"""
Spider Robot v3.1 - Eye Test Suite

Interactive test menu for eye control via Brain Daemon WebSocket.
Tests the Brain → Eye Service communication protocol.

Usage:
    python test_eyes.py [--ip 192.168.42.1] [--port 9000]
"""

import time
import math
import argparse
from spider_client import SpiderClient


def print_menu():
    print("\n" + "=" * 50)
    print("Spider Robot v3.1 - Eye Test Menu")
    print("=" * 50)
    print("1. Look center")
    print("2. Look left")
    print("3. Look right")
    print("4. Look up")
    print("5. Look down")
    print("6. Circle animation")
    print("7. Blink")
    print("8. Wink left")
    print("9. Wink right")
    print("-" * 50)
    print("m1. Mood: Normal")
    print("m2. Mood: Angry")
    print("m3. Mood: Happy")
    print("m4. Mood: Sleepy")
    print("-" * 50)
    print("i1. Idle ON")
    print("i0. Idle OFF")
    print("-" * 50)
    print("s.  Status")
    print("q.  Quit")
    print("=" * 50)


def circle_animation(client: SpiderClient, duration: float = 3.0, radius: float = 0.8):
    """Animate eyes in a circular pattern."""
    print(f"Running circle animation for {duration}s...")
    start = time.time()
    while (time.time() - start) < duration:
        t = (time.time() - start) * 2 * math.pi / 1.5  # 1.5s per circle
        x = radius * math.cos(t)
        y = radius * math.sin(t)
        client.eye_look(x, y)
        time.sleep(0.033)  # ~30 Hz
    client.eye_look(0, 0)  # Return to center
    print("Circle animation complete")


def main():
    parser = argparse.ArgumentParser(description="Spider Eye Test Suite")
    parser.add_argument("--ip", default="192.168.42.1", help="Brain daemon IP")
    parser.add_argument("--port", type=int, default=9000, help="Brain daemon port")
    args = parser.parse_args()

    client = SpiderClient(host=args.ip, port=args.port)

    if not client.connect():
        print("Failed to connect. Exiting.")
        return 1

    try:
        while True:
            print_menu()
            choice = input("Select option: ").strip().lower()

            if choice == "1":
                client.eye_look(0, 0)
                print("Looking center")
            elif choice == "2":
                client.eye_look(-1, 0)
                print("Looking left")
            elif choice == "3":
                client.eye_look(1, 0)
                print("Looking right")
            elif choice == "4":
                client.eye_look(0, -1)
                print("Looking up")
            elif choice == "5":
                client.eye_look(0, 1)
                print("Looking down")
            elif choice == "6":
                circle_animation(client)
            elif choice == "7":
                client.eye_blink()
                print("Blink triggered")
            elif choice == "8":
                client.eye_wink("left")
                print("Wink left triggered")
            elif choice == "9":
                client.eye_wink("right")
                print("Wink right triggered")
            elif choice == "m1":
                client.eye_mood("normal")
                print("Mood: Normal")
            elif choice == "m2":
                client.eye_mood("angry")
                print("Mood: Angry")
            elif choice == "m3":
                client.eye_mood("happy")
                print("Mood: Happy")
            elif choice == "m4":
                client.eye_mood("sleepy")
                print("Mood: Sleepy")
            elif choice == "i1":
                client.eye_idle(True)
                print("Idle ON")
            elif choice == "i0":
                client.eye_idle(False)
                print("Idle OFF")
            elif choice == "s":
                status = client.get_status()
                if status:
                    import json
                    print(f"Status: {json.dumps(status, indent=2)}")
                else:
                    print("No status response")
            elif choice == "q":
                print("Exiting...")
                break
            else:
                print(f"Unknown option: {choice}")

    except KeyboardInterrupt:
        print("\nInterrupted by user")
    finally:
        client.disconnect()

    return 0


if __name__ == "__main__":
    exit(main())
