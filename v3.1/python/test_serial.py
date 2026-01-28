#!/usr/bin/env python3
"""
Spider Robot v3.1 - Serial Command Tester

Tests serial commands by SSHing to the Duo and sending commands.
The serial port /dev/ttyS0 is for external microcontrollers.
"""

import subprocess
import time
import sys

DUO_IP = "192.168.42.1"
DUO_PASS = "milkv"

def ssh_cmd(cmd):
    """Run command on Duo via SSH"""
    full_cmd = f'sshpass -p {DUO_PASS} ssh -o StrictHostKeyChecking=no root@{DUO_IP} "{cmd}"'
    result = subprocess.run(['wsl', '-e', 'bash', '-c', full_cmd], 
                          capture_output=True, text=True, timeout=10)
    return result.stdout.strip(), result.returncode

def test_serial():
    print("=" * 50)
    print("SPIDER SERIAL COMMAND TEST")
    print("=" * 50)
    print()
    
    # Setup serial port
    print("[1] Configuring serial port...")
    ssh_cmd("stty -F /dev/ttyS0 115200 cs8 -cstopb -parenb")
    print("    /dev/ttyS0 @ 115200 8N1")
    
    # Test commands
    commands = [
        ("STATUS", "Get system status"),
        ("HELP", "List available commands"),
        ("SCAN 1500", "Center scan servo"),
        ("SCAN 1000", "Scan left"),
        ("SCAN 2000", "Scan right"),
        ("SCAN 1500", "Scan center"),
        ("EYE MOOD happy", "Set happy mood"),
        ("EYE BLINK", "Blink eyes"),
        ("EYE MOOD normal", "Set normal mood"),
    ]
    
    print()
    print("[2] Sending serial commands...")
    print("-" * 50)
    
    for cmd, desc in commands:
        print(f"    TX: {cmd:20} ({desc})")
        # Send command
        ssh_cmd(f"echo '{cmd}' > /dev/ttyS0")
        time.sleep(0.3)
    
    print("-" * 50)
    print()
    
    # Check brain log for received commands
    print("[3] Checking brain_daemon log...")
    output, _ = ssh_cmd("tail -30 /tmp/brain.log 2>/dev/null")
    if output:
        print(output)
    else:
        print("    (log empty - commands may be processed silently)")
    
    print()
    print("[4] Verify via WebSocket...")
    
    # Quick WebSocket test
    try:
        import websocket
        ws = websocket.create_connection(f'ws://{DUO_IP}:9000', timeout=3)
        ws.send('{"type":"status"}')
        time.sleep(0.2)
        ws.settimeout(1)
        resp = ws.recv()
        print(f"    WebSocket status: {resp}")
        ws.close()
    except Exception as e:
        print(f"    WebSocket error: {e}")
    
    print()
    print("=" * 50)
    print("TEST COMPLETE")
    print("=" * 50)
    print()
    print("Note: Serial responses go back to /dev/ttyS0.")
    print("Connect a microcontroller to see responses.")
    
    return 0

if __name__ == "__main__":
    sys.exit(test_serial())
