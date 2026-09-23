"""Read two ELF variables with one READ_MULTI request."""

from pathlib import Path
import sys

import serial


GUI_DIR = Path(__file__).resolve().parent.parent
PROJECT_ROOT = GUI_DIR.parents[1]
sys.path.insert(0, str(GUI_DIR))

from debug_protocol import read_variables  # noqa: E402
from elf_analysis import load_ram_symbols  # noqa: E402


PORT = "COM3"
ELF_PATH = PROJECT_ROOT / "Debug" / "stm32f103c8t6.elf"
VARIABLE_NAMES = (
    "DebugTest_U8",
    "DebugTest_U16",
    "DebugTest_U32",
    "DebugTest_S32",
    "DebugTest_Float32",
    "DebugTest_Counter",
    "DebugTest_Bytes",
)

symbols_by_name = {symbol.name: symbol for symbol in load_ram_symbols(ELF_PATH)}
symbols = [symbols_by_name[name] for name in VARIABLE_NAMES]
items = [(symbol.address, min(symbol.size, 8)) for symbol in symbols]


def print_exchange(request, response):
    print(f"TX : {request.hex(' ').upper()}")
    print(f"RX : {response.raw.hex(' ').upper()}")
    print(
        f"command={response.command}, status={response.status}, "
        f"sequence={response.sequence}, count={response.count}, "
        f"data_length={response.data_length}"
    )


with serial.Serial(PORT, baudrate=115200, timeout=3) as port:
    port.reset_input_buffer()
    values = read_variables(port, items, print_exchange)

for symbol, value in zip(symbols, values):
    number = int.from_bytes(value, byteorder="little", signed=False)
    print(f"{symbol.name:<20} 0x{symbol.address:08X} = {number} (0x{number:X})")
