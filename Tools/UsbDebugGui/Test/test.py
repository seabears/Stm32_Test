from pathlib import Path
import sys

import serial


GUI_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(GUI_DIR))

from debug_protocol import read_memory  # noqa: E402


PORT = "COM3"
ADDRESS = 0x08002000
LENGTH = 8


def print_exchange(request, response):
    print(f"요청 HEX : {request.hex(' ').upper()}")
    print(f"RAW HEX : {response.raw.hex(' ').upper()}")
    print(f"command  : {response.command}")
    print(f"status   : {response.status}")
    print(f"sequence : 0x{response.sequence:03X}")
    print(f"length   : {response.length}")


with serial.Serial(PORT, baudrate=115200, timeout=3) as ser:
    ser.reset_input_buffer()
    print("12바이트 응답을 기다리는 중...")
    data = read_memory(ser, ADDRESS, LENGTH, print_exchange)

print(f"data     : {data.hex(' ').upper()}")
