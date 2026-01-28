#!/usr/bin/env python3
"""
Spider Robot v3.1 - Python Brain Daemon
Full WebSocket server + hardware control.
Runs on Milk-V Duo as a working prototype.
"""

import asyncio
import json
import signal
import sys
import time
from typing import Optional, Dict, Any

# Configuration
WS_HOST = "0.0.0.0"
WS_PORT = 8765
I2C_BUS_SERVO = 1
I2C_BUS_LIDAR = 2
PCA9685_ADDR = 0x40
VL53L0X_ADDR = 0x29

# Try to import hardware libraries
try:
    import smbus2 as smbus
    HW_AVAILABLE = True
except ImportError:
    try:
        import smbus
        HW_AVAILABLE = True
    except ImportError:
        HW_AVAILABLE = False

# Try built-in mini_ws first (no dependencies), fall back to external
WS_AVAILABLE = False
try:
    from mini_ws import WebSocketServer
    WS_AVAILABLE = "builtin"
except ImportError:
    try:
        import websockets
        # Check if it actually works (Python 3.10+ required for websockets 16+)
        _ = websockets.serve
        WS_AVAILABLE = "external"
    except (ImportError, TypeError, AttributeError):
        WS_AVAILABLE = False


class PCA9685Driver:
    """PCA9685 16-channel PWM driver."""
    
    def __init__(self, bus_num=1, addr=0x40):
        self.addr = addr
        self.bus = None
        self.simulated = not HW_AVAILABLE
        
        if HW_AVAILABLE:
            try:
                self.bus = smbus.SMBus(bus_num)
                self._init_chip()
                print(f"[PCA9685] Initialized on I2C-{bus_num}")
            except Exception as e:
                print(f"[PCA9685] Hardware error: {e}, using simulation")
                self.simulated = True
        else:
            print("[PCA9685] No smbus, using simulation")
    
    def _init_chip(self):
        self.bus.write_byte_data(self.addr, 0x00, 0x80)  # Reset
        time.sleep(0.01)
        prescale = int(25000000.0 / (4096 * 50) - 1)  # 50Hz
        mode = self.bus.read_byte_data(self.addr, 0x00)
        self.bus.write_byte_data(self.addr, 0x00, (mode & 0x7F) | 0x10)
        self.bus.write_byte_data(self.addr, 0xFE, prescale)
        self.bus.write_byte_data(self.addr, 0x00, mode)
        time.sleep(0.005)
        self.bus.write_byte_data(self.addr, 0x00, mode | 0xA0)
    
    def set_angle(self, channel: int, angle: float):
        angle = max(0, min(180, angle))
        pulse_us = 500 + (2500 - 500) * angle / 180
        off_val = int(pulse_us * 4096 / 20000)
        
        if self.simulated:
            return
        
        reg = 0x06 + 4 * channel
        self.bus.write_i2c_block_data(self.addr, reg, [0, 0, off_val & 0xFF, off_val >> 8])


class VL53L0XDriver:
    """VL53L0X Time-of-Flight sensor."""
    
    def __init__(self, bus_num=2, addr=0x29):
        self.addr = addr
        self.bus = None
        self.simulated = not HW_AVAILABLE
        
        if HW_AVAILABLE:
            try:
                self.bus = smbus.SMBus(bus_num)
                model = self.bus.read_byte_data(self.addr, 0xC0)
                if model == 0xEE:
                    print(f"[VL53L0X] Initialized on I2C-{bus_num}")
                else:
                    print(f"[VL53L0X] Wrong ID 0x{model:02X}, using simulation")
                    self.simulated = True
            except Exception as e:
                print(f"[VL53L0X] Hardware error: {e}, using simulation")
                self.simulated = True
        else:
            print("[VL53L0X] No smbus, using simulation")
    
    def read_mm(self) -> int:
        if self.simulated:
            import random
            return random.randint(100, 2000)
        
        try:
            self.bus.write_byte_data(self.addr, 0x00, 0x01)
            time.sleep(0.05)
            data = self.bus.read_i2c_block_data(self.addr, 0x14 + 10, 2)
            return (data[0] << 8) | data[1]
        except:
            return -1


class SpiderState:
    """Robot state machine."""
    
    LEGS = {
        "RF": {"coxa": 0, "femur": 1, "tibia": 2},
        "RM": {"coxa": 3, "femur": 4, "tibia": 5},
        "RH": {"coxa": 6, "femur": 7, "tibia": 8},
        "LF": {"coxa": 9, "femur": 10, "tibia": 11},
    }
    
    POSES = {
        "stand": {"coxa": 90, "femur": 60, "tibia": 120},
        "sit": {"coxa": 90, "femur": 120, "tibia": 60},
        "neutral": {"coxa": 90, "femur": 90, "tibia": 90},
    }
    
    def __init__(self, servo_driver: PCA9685Driver):
        self.servos = servo_driver
        self.current_pose = "neutral"
        self.gait_running = False
        self.gait_task: Optional[asyncio.Task] = None
    
    def set_pose(self, pose_name: str):
        if pose_name not in self.POSES:
            return False
        
        angles = self.POSES[pose_name]
        for leg_name, joints in self.LEGS.items():
            for joint_name, channel in joints.items():
                self.servos.set_angle(channel, angles.get(joint_name, 90))
        
        self.current_pose = pose_name
        print(f"[State] Pose: {pose_name}")
        return True
    
    async def run_gait(self, gait_name: str, speed: int = 50):
        self.gait_running = True
        delay = 0.5 * (100 - speed) / 100 + 0.1
        
        print(f"[State] Gait: {gait_name} @ speed {speed}")
        
        step = 0
        while self.gait_running:
            step += 1
            print(f"[Gait] Step {step}")
            # Tripod 1
            for leg in ["RF", "RH"]:
                for joint, ch in self.LEGS[leg].items():
                    self.servos.set_angle(ch, 70 if joint == "femur" else 90)
            await asyncio.sleep(delay)
            
            # Tripod 2
            for leg in ["RM", "LF"]:
                for joint, ch in self.LEGS[leg].items():
                    self.servos.set_angle(ch, 70 if joint == "femur" else 90)
            await asyncio.sleep(delay)
            
            if step >= 20:  # Safety limit
                break
        
        self.gait_running = False
        print("[State] Gait stopped")
    
    def stop_gait(self):
        self.gait_running = False


class BrainDaemon:
    """Main Brain Daemon with WebSocket server."""
    
    def __init__(self):
        self.servos = PCA9685Driver(I2C_BUS_SERVO, PCA9685_ADDR)
        self.lidar = VL53L0XDriver(I2C_BUS_LIDAR, VL53L0X_ADDR)
        self.state = SpiderState(self.servos)
        self.clients: set = set()
        self.running = True
        self.start_time = time.time()
    
    async def handle_command(self, data: Dict[str, Any]) -> Dict[str, Any]:
        cmd = data.get("cmd", "")
        
        if cmd == "status":
            return {
                "status": "ok",
                "pose": self.state.current_pose,
                "gait_running": self.state.gait_running,
                "uptime": int(time.time() - self.start_time),
                "hw_servo": not self.servos.simulated,
                "hw_lidar": not self.lidar.simulated,
            }
        
        elif cmd == "pose":
            pose = data.get("pose", "neutral")
            ok = self.state.set_pose(pose)
            return {"ok": ok, "pose": pose}
        
        elif cmd == "gait":
            gait = data.get("gait", "walk")
            speed = data.get("speed", 50)
            if self.state.gait_task and not self.state.gait_task.done():
                self.state.stop_gait()
                await asyncio.sleep(0.2)
            self.state.gait_task = asyncio.create_task(
                self.state.run_gait(gait, speed)
            )
            return {"ok": True, "gait": gait}
        
        elif cmd == "stop":
            self.state.stop_gait()
            return {"ok": True}
        
        elif cmd == "lidar":
            dist = self.lidar.read_mm()
            return {"distance_mm": dist}
        
        elif cmd == "servo":
            ch = data.get("channel", 0)
            angle = data.get("angle", 90)
            self.servos.set_angle(ch, angle)
            return {"ok": True, "channel": ch, "angle": angle}
        
        else:
            return {"error": f"Unknown command: {cmd}"}
    
    async def ws_handler(self, websocket, path):
        self.clients.add(websocket)
        addr = websocket.remote_address
        print(f"[WS] Client connected: {addr}")
        
        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    print(f"[WS] <- {data}")
                    response = await self.handle_command(data)
                    print(f"[WS] -> {response}")
                    await websocket.send(json.dumps(response))
                except json.JSONDecodeError:
                    await websocket.send(json.dumps({"error": "Invalid JSON"}))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.clients.discard(websocket)
            print(f"[WS] Client disconnected: {addr}")
    
    def _sync_message_handler(self, msg: str) -> str:
        """Synchronous wrapper for builtin WebSocket server."""
        try:
            data = json.loads(msg)
            print(f"[WS] <- {data}")
            # Run async handler in sync context
            loop = asyncio.new_event_loop()
            response = loop.run_until_complete(self.handle_command(data))
            loop.close()
            print(f"[WS] -> {response}")
            return json.dumps(response)
        except json.JSONDecodeError:
            return json.dumps({"error": "Invalid JSON"})
        except Exception as e:
            return json.dumps({"error": str(e)})
    
    async def run(self):
        print("=" * 50)
        print("Spider Robot v3.1 - Python Brain Daemon")
        print("=" * 50)
        print(f"WebSocket: ws://{WS_HOST}:{WS_PORT}")
        print(f"Hardware - Servos: {'REAL' if not self.servos.simulated else 'SIM'}")
        print(f"Hardware - LiDAR:  {'REAL' if not self.lidar.simulated else 'SIM'}")
        print(f"WS Mode: {WS_AVAILABLE if WS_AVAILABLE else 'console-only'}")
        print("=" * 50)
        
        self.state.set_pose("neutral")
        
        if not WS_AVAILABLE:
            print("\n[ERROR] No WebSocket support!")
            print("Copy mini_ws.py to /root/ or install websockets")
            print("\nRunning in console-only mode...")
            await self.console_mode()
            return
        
        if WS_AVAILABLE == "builtin":
            # Use built-in mini WebSocket server
            print("\nUsing built-in WebSocket server...")
            ws_server = WebSocketServer(WS_HOST, WS_PORT)
            
            import threading
            ws_thread = threading.Thread(
                target=ws_server.start,
                args=(self._sync_message_handler,)
            )
            ws_thread.daemon = True
            ws_thread.start()
            
            print("Server running. Press Ctrl+C to stop.\n")
            while self.running:
                await asyncio.sleep(1)
            
            ws_server.stop()
        else:
            # Use external websockets library
            async with websockets.serve(self.ws_handler, WS_HOST, WS_PORT):
                print("\nServer running. Press Ctrl+C to stop.\n")
                while self.running:
                    await asyncio.sleep(1)
    
    async def console_mode(self):
        """Fallback if websockets not available."""
        print("\nConsole mode. Commands: status, stand, sit, walk, stop, quit")
        
        loop = asyncio.get_event_loop()
        while self.running:
            try:
                cmd = await loop.run_in_executor(None, input, "> ")
                cmd = cmd.strip().lower()
                
                if cmd == "quit":
                    break
                elif cmd == "status":
                    resp = await self.handle_command({"cmd": "status"})
                elif cmd in ["stand", "sit", "neutral"]:
                    resp = await self.handle_command({"cmd": "pose", "pose": cmd})
                elif cmd == "walk":
                    resp = await self.handle_command({"cmd": "gait", "gait": "walk"})
                elif cmd == "stop":
                    resp = await self.handle_command({"cmd": "stop"})
                elif cmd == "lidar":
                    resp = await self.handle_command({"cmd": "lidar"})
                else:
                    resp = {"error": "Unknown command"}
                
                print(json.dumps(resp, indent=2))
            except EOFError:
                break
    
    def shutdown(self):
        self.running = False
        self.state.stop_gait()
        print("\n[Brain] Shutting down...")


def main():
    daemon = BrainDaemon()
    
    def sig_handler(sig, frame):
        daemon.shutdown()
    
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
    
    try:
        asyncio.run(daemon.run())
    except KeyboardInterrupt:
        pass
    
    print("[Brain] Goodbye!")


if __name__ == "__main__":
    main()
