#!/usr/bin/env python3
"""WebSocket test client for Spider Brain Daemon."""

import asyncio
import websockets
import json

DUO_IP = "192.168.42.1"
WS_PORT = 8080  # C++ daemon uses 8080

async def test():
    uri = f"ws://{DUO_IP}:{WS_PORT}"
    print(f"Connecting to {uri}...")
    
    async with websockets.connect(uri) as ws:
        print("Connected!\n")
        
        # Test status
        print(">>> status")
        await ws.send('{"cmd":"status"}')
        resp = await ws.recv()
        print(json.dumps(json.loads(resp), indent=2))
        print()
        
        # Test pose
        print(">>> pose: stand")
        await ws.send('{"cmd":"pose","pose":"stand"}')
        resp = await ws.recv()
        print(json.dumps(json.loads(resp), indent=2))
        print()
        
        # Test lidar
        print(">>> lidar")
        await ws.send('{"cmd":"lidar"}')
        resp = await ws.recv()
        print(json.dumps(json.loads(resp), indent=2))
        print()
        
        # Test walk (brief)
        print(">>> gait: walk")
        await ws.send('{"cmd":"gait","gait":"walk","speed":80}')
        resp = await ws.recv()
        print(json.dumps(json.loads(resp), indent=2))
        
        await asyncio.sleep(2)
        
        # Stop
        print("\n>>> stop")
        await ws.send('{"cmd":"stop"}')
        resp = await ws.recv()
        print(json.dumps(json.loads(resp), indent=2))
        
        print("\nTest complete!")

if __name__ == "__main__":
    asyncio.run(test())
