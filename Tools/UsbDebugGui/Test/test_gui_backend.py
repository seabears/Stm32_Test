"""Hardware integration test for the same backend used by main.py."""

from pathlib import Path
import sys

import serial


GUI_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(GUI_DIR))

from debug_protocol import read_memory  # noqa: E402


PORT = "COM3"
ADDRESS = 0x08002000
LENGTH = 8


def print_exchange(request, response) -> None:
    print(f"TX  : {request.hex(' ').upper()}")
    print(f"RX  : {response.raw.hex(' ').upper()}")
    print(
        f"HDR : command={response.command}, status={response.status}, "
        f"sequence={response.sequence}, length={response.length}"
    )


with serial.Serial(PORT, baudrate=115200, timeout=3) as port:
    port.reset_input_buffer()
    data = read_memory(port, ADDRESS, LENGTH, print_exchange)

print(f"DATA: {data.hex(' ').upper()}")
