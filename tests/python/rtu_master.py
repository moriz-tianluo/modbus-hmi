"""测试用 Modbus RTU 主站报文构造与解析，不依赖真实串口。"""

import sys
from pathlib import Path

_SIM_DIR = Path(__file__).resolve().parents[2] / "python"
if str(_SIM_DIR) not in sys.path:
    sys.path.insert(0, str(_SIM_DIR))

from slave_sim import crc16, with_crc  # noqa: E402

# 功能码
READ_COILS = 0x01
READ_DISCRETE_INPUTS = 0x02
READ_HOLDING = 0x03
READ_INPUT = 0x04
WRITE_COIL = 0x05
WRITE_REGISTER = 0x06
WRITE_COILS = 0x0F
WRITE_REGISTERS = 0x10


def _u16(value: int) -> bytes:
    return bytes(((value >> 8) & 0xFF, value & 0xFF))


def _request(slave: int, func: int, payload: bytes) -> bytes:
    return with_crc(bytes((slave, func)) + payload)


def read_request(slave: int, func: int, start: int, count: int) -> bytes:
    """构造 01 / 02 / 03 / 04 读请求。"""
    return _request(slave, func, _u16(start) + _u16(count))


def write_single_register(slave: int, address: int, value: int) -> bytes:
    return _request(slave, WRITE_REGISTER, _u16(address) + _u16(value))


def write_multiple_registers(slave: int, start: int, values) -> bytes:
    body = _u16(start) + _u16(len(values)) + bytes((len(values) * 2,))
    body += b"".join(_u16(value) for value in values)
    return _request(slave, WRITE_REGISTERS, body)


def write_single_coil(slave: int, address: int, on: bool) -> bytes:
    value = 0xFF00 if on else 0x0000
    return _request(slave, WRITE_COIL, _u16(address) + _u16(value))


def write_multiple_coils(slave: int, start: int, states) -> bytes:
    byte_count = (len(states) + 7) // 8
    raw = bytearray(byte_count)
    for index, state in enumerate(states):
        if state:
            raw[index // 8] |= 1 << (index % 8)
    body = _u16(start) + _u16(len(states)) + bytes((byte_count,)) + bytes(raw)
    return _request(slave, WRITE_COILS, body)


def is_valid(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    return crc16(frame[:-2]) == (frame[-2] | (frame[-1] << 8))


def exception_code(frame: bytes):
    """返回异常码；正常应答返回 None。"""
    if len(frame) >= 5 and frame[1] & 0x80:
        return frame[2]
    return None


def register_values(frame: bytes) -> list:
    """解析 03 / 04 应答中的寄存器值列表。"""
    count = frame[2] // 2
    return [(frame[3 + index * 2] << 8) | frame[4 + index * 2]
            for index in range(count)]


def bit_values(frame: bytes, count: int) -> list:
    """解析 01 / 02 应答中的位列表。"""
    return [bool(frame[3 + index // 8] & (1 << (index % 8)))
            for index in range(count)]
