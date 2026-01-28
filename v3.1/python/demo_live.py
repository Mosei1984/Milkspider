#!/usr/bin/env python3
"""
Spider Robot v3.1 - Live System Demo

Shows all components working together via the real Brain Daemon.
No hardware needed - demonstrates the software communication chain.
"""

import time
import sys
from spider_client import SpiderClient


def demo_system():
    print("=" * 60)
    print("SPIDER ROBOT v3.1 - LIVE SYSTEM DEMO")
    print("=" * 60)
    print()
    
    client = SpiderClient()
    
    if not client.connect():
        print("ERROR: Cannot connect to Brain Daemon")
        print("Make sure brain_daemon is running on 192.168.42.1:9000")
        return 1
    
    print()
    print("--- Step 1: Query System Status ---")
    status = client.get_status()
    print(f"Status: {status}")
    
    print()
    print("--- Step 2: Get Current Servo Positions ---")
    resp = client.send_command({"type": "get_servos"})
    if resp:
        print(f"Servos: {resp}")
    else:
        print("(No response - servos at neutral)")
    
    print()
    print("--- Step 3: Eye Mood Sequence ---")
    moods = ["normal", "happy", "angry", "sleepy", "normal"]
    for mood in moods:
        client.eye_mood(mood)
        print(f"  Mood: {mood}")
        time.sleep(0.4)
    
    print()
    print("--- Step 4: Eye Look Around ---")
    positions = [
        (0, 0, "center"),
        (-0.8, 0, "left"),
        (0.8, 0, "right"),
        (0, -0.6, "up"),
        (0, 0.6, "down"),
        (0, 0, "center"),
    ]
    for x, y, name in positions:
        client.eye_look(x, y)
        print(f"  Looking: {name}")
        time.sleep(0.3)
    
    print()
    print("--- Step 5: Blink & Wink ---")
    client.eye_blink()
    print("  Blink!")
    time.sleep(0.4)
    client.eye_wink("left")
    print("  Wink left")
    time.sleep(0.4)
    client.eye_wink("right")
    print("  Wink right")
    time.sleep(0.3)
    
    print()
    print("--- Step 6: Scan Servo Test ---")
    print("  Moving scan servo (CH12)...")
    client.scan_angle(45)
    print("  Position: 45°")
    time.sleep(0.3)
    client.scan_angle(135)
    print("  Position: 135°")
    time.sleep(0.3)
    client.scan_center()
    print("  Position: 90° (center)")
    
    print()
    print("--- Step 7: Distance Sensor ---")
    distance = client.get_distance()
    if distance is not None:
        print(f"  Distance: {distance} mm")
    else:
        print("  Distance: not available (sensor not connected)")
    
    print()
    print("--- Step 8: Simulate Motion Command ---")
    print("  Sending neutral pose to all 13 servos...")
    neutral = [1500] * 13
    resp = client.send_command({"type": "move", "t_ms": 500, "us": neutral})
    if resp:
        print(f"  Response: {resp}")
    else:
        print("  Sent (no ack expected)")
    
    print()
    print("--- Step 9: Enable Idle Animation ---")
    client.eye_idle(True)
    print("  Idle animation enabled")
    
    client.disconnect()
    
    print()
    print("=" * 60)
    print("DEMO COMPLETE!")
    print("=" * 60)
    print()
    print("Communication chain:")
    print("  Python Client")
    print("       | WebSocket")
    print("       v")
    print("  Brain Daemon (Linux)")
    print("       | Shared Memory + Mailbox")
    print("       v")
    print("  Muscle Runtime (FreeRTOS)")
    print("       | I2C")
    print("       v")
    print("  PCA9685 -> Servos (when connected)")
    print()
    print("  Brain Daemon -> Unix Socket -> Eye Service -> SPI -> Displays")
    print()
    
    return 0


if __name__ == "__main__":
    sys.exit(demo_system())
