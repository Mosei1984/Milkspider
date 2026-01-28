#!/usr/bin/env python3
"""
Spider Robot v3.1 - VL53L0X Time-of-Flight Sensor Test
Tests the ToF distance sensor on I2C-2.
"""

import time

VL53L0X_ADDR = 0x29

# Key registers
REG_IDENTIFICATION_MODEL_ID = 0xC0
REG_SYSRANGE_START = 0x00
REG_RESULT_RANGE_STATUS = 0x14

class VL53L0X:
    def __init__(self, bus_num=2, addr=VL53L0X_ADDR):
        self.addr = addr
        try:
            import smbus2 as smbus
        except ImportError:
            import smbus
        self.bus = smbus.SMBus(bus_num)
        self._verify_device()

    def _verify_device(self):
        """Check device ID."""
        model_id = self.bus.read_byte_data(self.addr, REG_IDENTIFICATION_MODEL_ID)
        if model_id != 0xEE:
            raise ValueError(f"Invalid model ID: 0x{model_id:02X} (expected 0xEE)")
        print(f"VL53L0X found: model_id=0x{model_id:02X}")

    def _write_byte(self, reg, val):
        self.bus.write_byte_data(self.addr, reg, val)

    def _read_byte(self, reg):
        return self.bus.read_byte_data(self.addr, reg)

    def _read_word(self, reg):
        data = self.bus.read_i2c_block_data(self.addr, reg, 2)
        return (data[0] << 8) | data[1]

    def init_sensor(self):
        """Basic initialization (simplified - full init requires 60+ register writes)."""
        # This is a minimal init. For production, use the ST API sequence.
        self._write_byte(0x88, 0x00)
        self._write_byte(0x80, 0x01)
        self._write_byte(0xFF, 0x01)
        self._write_byte(0x00, 0x00)
        time.sleep(0.01)
        self._write_byte(0x00, 0x01)
        self._write_byte(0xFF, 0x00)
        self._write_byte(0x80, 0x00)
        print("VL53L0X basic init complete")

    def read_range_single(self):
        """Perform single-shot range measurement."""
        # Start measurement
        self._write_byte(REG_SYSRANGE_START, 0x01)
        
        # Wait for measurement to complete (bit 0 clears)
        timeout = 50
        while timeout > 0:
            status = self._read_byte(REG_SYSRANGE_START)
            if (status & 0x01) == 0:
                break
            time.sleep(0.01)
            timeout -= 1
        
        if timeout == 0:
            return -1  # Timeout
        
        # Wait for result ready
        timeout = 50
        while timeout > 0:
            range_status = self._read_byte(REG_RESULT_RANGE_STATUS)
            if (range_status & 0x01):
                break
            time.sleep(0.01)
            timeout -= 1
        
        if timeout == 0:
            return -2  # Result timeout
        
        # Read range value
        range_mm = self._read_word(REG_RESULT_RANGE_STATUS + 10)
        
        # Clear interrupt
        self._write_byte(0x0B, 0x01)
        
        return range_mm


def test_distance():
    """Continuous distance reading test."""
    print("=== VL53L0X Distance Test ===\n")
    
    try:
        sensor = VL53L0X(bus_num=2)
        sensor.init_sensor()
    except Exception as e:
        print(f"ERROR: {e}")
        return False

    print("\nReading distance (Ctrl+C to stop)...\n")
    try:
        while True:
            dist = sensor.read_range_single()
            if dist < 0:
                print(f"Error: {dist}")
            elif dist > 2000:
                print("Out of range (>2m)")
            else:
                bar = "#" * min(50, dist // 20)
                print(f"\r{dist:4d} mm  [{bar:<50}]", end="", flush=True)
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n\nStopped")
    
    return True


if __name__ == "__main__":
    test_distance()
