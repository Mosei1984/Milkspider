#!/usr/bin/env python3
import websocket
import time

ws = websocket.create_connection('ws://192.168.42.1:9000', timeout=5)
print("Connected!")

# Test status
ws.send('{"type":"status"}')
time.sleep(0.3)
ws.settimeout(1)
try:
    r = ws.recv()
    print(f"Status: {r}")
except:
    print("Status: no response")

# Test get_servos
ws.send('{"type":"get_servos"}')
time.sleep(0.3)
try:
    r = ws.recv()
    print(f"Servos: {r}")
except:
    print("Servos: no response")

# Test scan
ws.send('{"type":"scan","us":1200}')
time.sleep(0.3)
try:
    r = ws.recv()
    print(f"Scan: {r}")
except:
    print("Scan: no response")

# Test move
ws.send('{"type":"move","t_ms":100,"us":[1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500]}')
time.sleep(0.3)
try:
    r = ws.recv()
    print(f"Move: {r}")
except:
    print("Move: no response")

ws.close()
print("Done!")
