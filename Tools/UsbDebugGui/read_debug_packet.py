import argparse
import struct
import time

import serial
from serial.tools import list_ports


PACKET_SIZE = 12
STM32_VID = 0x0483
STM32_PID = 0x5740


def find_stm32_port():
    for port in list_ports.comports():
        if port.vid == STM32_VID and port.pid == STM32_PID:
            return port.device
    return None


def read_exact(port, size, timeout_seconds):
    received = bytearray()
    deadline = time.monotonic() + timeout_seconds

    while len(received) < size and time.monotonic() < deadline:
        chunk = port.read(size - len(received))
        if chunk:
            received.extend(chunk)

    return bytes(received)


def main():
    parser = argparse.ArgumentParser(
        description="STM32 DebugResponsePacket 12바이트 수신 테스트"
    )
    parser.add_argument("--port", help="예: COM5. 생략하면 0483:5740 자동 검색")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    port_name = args.port or find_stm32_port()
    if port_name is None:
        raise SystemExit("STM32 CDC(0483:5740) COM 포트를 찾지 못했습니다.")

    print(f"수신 대기: {port_name}")
    print("데이터가 오지 않으면 이 상태에서 보드를 Reset 하세요.")

    with serial.Serial(port_name, 115200, timeout=0.1) as port:
        raw = read_exact(port, PACKET_SIZE, args.timeout)

    if len(raw) != PACKET_SIZE:
        raise SystemExit(
            f"수신 시간 초과: {len(raw)}/{PACKET_SIZE}바이트 수신, raw={raw.hex(' ')}"
        )

    header = struct.unpack_from("<I", raw, 0)[0]
    command = header & 0xFF
    status = (header >> 8) & 0x07
    sequence = (header >> 11) & 0x07FF
    length = (header >> 22) & 0x0F
    data = raw[4:4 + min(length, 8)]

    command_text = chr(command) if 0x20 <= command <= 0x7E else "?"

    print(f"raw      : {raw.hex(' ')}")
    print(f"command  : 0x{command:02X} ('{command_text}')")
    print(f"status   : {status}")
    print(f"sequence : {sequence} (0x{sequence:03X})")
    print(f"length   : {length}")
    print(f"data     : {data.hex(' ')}")


if __name__ == "__main__":
    main()
