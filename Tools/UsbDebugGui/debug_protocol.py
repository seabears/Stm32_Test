"""Encoding and decoding for the STM32 10-byte request / 12-byte response protocol."""

from __future__ import annotations

from dataclasses import dataclass
import struct
import time
from typing import Callable, Sequence


READ_COMMAND = 3
MULTI_READ_COMMAND = 5
STATUS_OK = 1
REQUEST_SIZE = 10
RESPONSE_SIZE = 12
MAX_DATA_SIZE = 8
MULTI_MAX_ITEMS = 8
MULTI_MAX_DATA_SIZE = 58
MULTI_RESPONSE_HEADER_SIZE = 6


class DebugProtocolError(RuntimeError):
    """Raised when a debug packet is malformed or reports an error."""


@dataclass(frozen=True)
class DebugResponse:
    command: int
    status: int
    sequence: int
    length: int
    data: bytes
    raw: bytes


@dataclass(frozen=True)
class DebugMultiResponse:
    command: int
    status: int
    sequence: int
    count: int
    data_length: int
    data: bytes
    raw: bytes


def build_read_request(address: int, length: int) -> bytes:
    """Build: [command:1][address ASCII hex:8][length ASCII hex:1]."""
    if not 0 <= address <= 0xFFFFFFFF:
        raise ValueError("주소는 0x00000000~0xFFFFFFFF 범위여야 합니다.")
    if not 1 <= length <= MAX_DATA_SIZE:
        raise ValueError("한 요청의 길이는 1~8바이트여야 합니다.")

    packet = bytes([READ_COMMAND]) + f"{address:08X}{length:X}".encode("ascii")
    if len(packet) != REQUEST_SIZE:
        raise AssertionError("요청 패킷 크기가 10바이트가 아닙니다.")
    return packet


def parse_response(raw: bytes) -> DebugResponse:
    """Decode the little-endian 32-bit bit-field header and 8-byte payload."""
    if len(raw) != RESPONSE_SIZE:
        raise DebugProtocolError(
            f"응답 길이가 {len(raw)}바이트입니다. {RESPONSE_SIZE}바이트가 필요합니다."
        )

    header = struct.unpack_from("<I", raw, 0)[0]
    length = (header >> 22) & 0x0F
    if length > MAX_DATA_SIZE:
        raise DebugProtocolError(f"응답 length 필드가 잘못되었습니다: {length}")

    return DebugResponse(
        command=header & 0xFF,
        status=(header >> 8) & 0x07,
        sequence=(header >> 11) & 0x7FF,
        length=length,
        data=raw[4 : 4 + length],
        raw=raw,
    )


def build_multi_read_request(items: Sequence[tuple[int, int]]) -> bytes:
    """Build: [command:1][count:1] + count * [address LE32][length:1]."""
    if not 1 <= len(items) <= MULTI_MAX_ITEMS:
        raise ValueError(f"다중 읽기는 1~{MULTI_MAX_ITEMS}개 변수를 지원합니다.")

    total_length = 0
    packet = bytearray([MULTI_READ_COMMAND, len(items)])
    for address, length in items:
        if not 0 <= address <= 0xFFFFFFFF:
            raise ValueError("주소는 0x00000000~0xFFFFFFFF 범위여야 합니다.")
        if not 1 <= length <= MAX_DATA_SIZE:
            raise ValueError("변수 하나의 길이는 1~8바이트여야 합니다.")
        total_length += length
        packet.extend(struct.pack("<IB", address, length))

    if total_length > MULTI_MAX_DATA_SIZE:
        raise ValueError(
            f"다중 응답 데이터는 최대 {MULTI_MAX_DATA_SIZE}바이트입니다."
        )
    return bytes(packet)


def parse_multi_response(raw: bytes) -> DebugMultiResponse:
    if len(raw) < MULTI_RESPONSE_HEADER_SIZE:
        raise DebugProtocolError("다중 응답 헤더가 6바이트보다 짧습니다.")

    data_length = raw[5]
    if data_length > MULTI_MAX_DATA_SIZE:
        raise DebugProtocolError(f"다중 응답 데이터 길이가 잘못되었습니다: {data_length}")
    if len(raw) != MULTI_RESPONSE_HEADER_SIZE + data_length:
        raise DebugProtocolError(
            f"다중 응답 길이가 헤더와 다릅니다: {len(raw)}바이트"
        )

    return DebugMultiResponse(
        command=raw[0],
        status=raw[1],
        sequence=struct.unpack_from("<H", raw, 2)[0] & 0x7FF,
        count=raw[4],
        data_length=data_length,
        data=raw[MULTI_RESPONSE_HEADER_SIZE:],
        raw=raw,
    )


def read_exact(port, size: int, timeout_seconds: float) -> bytes:
    """Read exactly ``size`` bytes from a pyserial-compatible port."""
    deadline = time.monotonic() + timeout_seconds
    received = bytearray()
    while len(received) < size and time.monotonic() < deadline:
        received.extend(port.read(size - len(received)))
    if len(received) != size:
        raise DebugProtocolError(
            f"응답 시간 초과: {len(received)}/{size}바이트 수신"
        )
    return bytes(received)


def read_memory(
    port,
    address: int,
    length: int,
    on_exchange: Callable[[bytes, DebugResponse], None] | None = None,
) -> bytes:
    """Read target memory, splitting it into protocol-sized requests."""
    if length <= 0:
        raise ValueError("읽기 길이는 1바이트 이상이어야 합니다.")

    result = bytearray()
    while len(result) < length:
        chunk_address = address + len(result)
        chunk_size = min(MAX_DATA_SIZE, length - len(result))
        request = build_read_request(chunk_address, chunk_size)
        port.write(request)
        port.flush()

        response = parse_response(read_exact(port, RESPONSE_SIZE, 0.5))
        if on_exchange is not None:
            on_exchange(request, response)
        if response.command != READ_COMMAND:
            raise DebugProtocolError(f"예상하지 않은 command={response.command}")
        if response.status != STATUS_OK:
            raise DebugProtocolError(f"STM32가 status={response.status}를 반환했습니다.")
        if response.length != chunk_size:
            raise DebugProtocolError(
                f"요청 {chunk_size}바이트, 응답 {response.length}바이트입니다."
            )
        result.extend(response.data)

    return bytes(result)


def read_variables(
    port,
    items: Sequence[tuple[int, int]],
    on_exchange: Callable[[bytes, DebugMultiResponse], None] | None = None,
) -> list[bytes]:
    """Read several non-contiguous variables with one USB request/response."""
    request = build_multi_read_request(items)
    port.write(request)
    port.flush()

    header = read_exact(port, MULTI_RESPONSE_HEADER_SIZE, 0.5)
    payload = read_exact(port, header[5], 0.5) if header[5] > 0 else b""
    response = parse_multi_response(header + payload)
    if on_exchange is not None:
        on_exchange(request, response)

    if response.command != MULTI_READ_COMMAND:
        raise DebugProtocolError(f"예상하지 않은 command={response.command}")
    if response.status != STATUS_OK:
        raise DebugProtocolError(f"STM32가 status={response.status}를 반환했습니다.")
    if response.count != len(items):
        raise DebugProtocolError(
            f"요청 변수 {len(items)}개, 응답 변수 {response.count}개입니다."
        )
    expected_length = sum(length for _, length in items)
    if response.data_length != expected_length:
        raise DebugProtocolError(
            f"요청 데이터 {expected_length}바이트, "
            f"응답 데이터 {response.data_length}바이트입니다."
        )

    values = []
    offset = 0
    for _, length in items:
        values.append(response.data[offset : offset + length])
        offset += length
    return values
