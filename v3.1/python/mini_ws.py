#!/usr/bin/env python3
"""
Minimal WebSocket server - no external dependencies.
Based on RFC 6455. Works on Milk-V Duo without pip.
"""

import socket
import hashlib
import base64
import struct
import threading
import json

WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

def create_accept_key(key: str) -> str:
    sha1 = hashlib.sha1((key + WS_MAGIC).encode()).digest()
    return base64.b64encode(sha1).decode()

def parse_frame(data: bytes):
    if len(data) < 2:
        return None, None, data
    
    fin = (data[0] >> 7) & 1
    opcode = data[0] & 0x0F
    masked = (data[1] >> 7) & 1
    payload_len = data[1] & 0x7F
    
    offset = 2
    if payload_len == 126:
        if len(data) < 4:
            return None, None, data
        payload_len = struct.unpack(">H", data[2:4])[0]
        offset = 4
    elif payload_len == 127:
        if len(data) < 10:
            return None, None, data
        payload_len = struct.unpack(">Q", data[2:10])[0]
        offset = 10
    
    if masked:
        if len(data) < offset + 4 + payload_len:
            return None, None, data
        mask = data[offset:offset+4]
        offset += 4
        payload = bytearray(data[offset:offset+payload_len])
        for i in range(len(payload)):
            payload[i] ^= mask[i % 4]
        payload = bytes(payload)
    else:
        if len(data) < offset + payload_len:
            return None, None, data
        payload = data[offset:offset+payload_len]
    
    remaining = data[offset+payload_len:]
    return opcode, payload, remaining

def encode_frame(data: bytes, opcode: int = 1) -> bytes:
    frame = bytearray()
    frame.append(0x80 | opcode)  # FIN + opcode
    
    length = len(data)
    if length <= 125:
        frame.append(length)
    elif length <= 65535:
        frame.append(126)
        frame.extend(struct.pack(">H", length))
    else:
        frame.append(127)
        frame.extend(struct.pack(">Q", length))
    
    frame.extend(data)
    return bytes(frame)


class WebSocketServer:
    def __init__(self, host: str = "0.0.0.0", port: int = 8765):
        self.host = host
        self.port = port
        self.clients = []
        self.on_message = None
        self.running = False
        self.server_socket = None
    
    def start(self, on_message):
        self.on_message = on_message
        self.running = True
        
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        self.server_socket.settimeout(1.0)
        
        print(f"[WS] Server listening on ws://{self.host}:{self.port}")
        
        while self.running:
            try:
                client, addr = self.server_socket.accept()
                print(f"[WS] Connection from {addr}")
                t = threading.Thread(target=self._handle_client, args=(client, addr))
                t.daemon = True
                t.start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[WS] Accept error: {e}")
    
    def stop(self):
        self.running = False
        if self.server_socket:
            self.server_socket.close()
    
    def _handle_client(self, client: socket.socket, addr):
        try:
            # Handshake
            request = client.recv(4096).decode()
            
            key = None
            for line in request.split("\r\n"):
                if line.lower().startswith("sec-websocket-key:"):
                    key = line.split(":", 1)[1].strip()
                    break
            
            if not key:
                client.close()
                return
            
            accept = create_accept_key(key)
            response = (
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                f"Sec-WebSocket-Accept: {accept}\r\n"
                "\r\n"
            )
            client.send(response.encode())
            
            self.clients.append(client)
            buffer = b""
            
            while self.running:
                try:
                    client.settimeout(1.0)
                    data = client.recv(4096)
                    if not data:
                        break
                    
                    buffer += data
                    
                    while buffer:
                        opcode, payload, buffer = parse_frame(buffer)
                        if opcode is None:
                            break
                        
                        if opcode == 8:  # Close
                            client.send(encode_frame(b"", 8))
                            raise ConnectionError("Close frame")
                        elif opcode == 9:  # Ping
                            client.send(encode_frame(payload, 10))
                        elif opcode == 1:  # Text
                            msg = payload.decode()
                            if self.on_message:
                                response = self.on_message(msg)
                                if response:
                                    client.send(encode_frame(response.encode(), 1))
                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"[WS] Client error: {e}")
                    break
        
        except Exception as e:
            print(f"[WS] Handler error: {e}")
        
        finally:
            if client in self.clients:
                self.clients.remove(client)
            client.close()
            print(f"[WS] Disconnected: {addr}")


# Test/demo
if __name__ == "__main__":
    def echo_handler(msg):
        print(f"[Echo] Received: {msg}")
        try:
            data = json.loads(msg)
            return json.dumps({"echo": data, "status": "ok"})
        except:
            return json.dumps({"error": "invalid json"})
    
    server = WebSocketServer("0.0.0.0", 8765)
    try:
        server.start(echo_handler)
    except KeyboardInterrupt:
        server.stop()
        print("\nServer stopped")
