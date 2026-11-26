"""Modbus RTU 从站模拟器（联调 / 自动化测试用）。

支持的功能码：
  01 读线圈、02 读离散输入、03 读保持寄存器、04 读输入寄存器
  05 写单线圈、06 写单寄存器、0F 写多线圈、10 写多寄存器

既可以接真实串口与上位机联调，也可以直接调用 ModbusSlaveSimulator.feed()
在无硬件、无虚拟串口的环境下做协议级自动化测试（见 tests/python）。

### 常用命令
# 1) 正常从站：单从站，8 个保持寄存器
#python slave_sim.py --port COM10 --slave 1 --regs 8

# 2) 多从站：1/2/3 共用一个串口，配合上位机通道里的「从站地址」
#python slave_sim.py --port COM10 --slave-ids 1,2,3 --regs 16 --coils 64

# 3) 故障注入
#python slave_sim.py --port COM10 --timeout-rate 0.2   # 20% 不响应
#python slave_sim.py --port COM10 --exc-rate 0.2       # 20% 返回异常码
#python slave_sim.py --port COM10 --bad-crc-rate 0.2   # 20% 错误 CRC
#python slave_sim.py --port COM10 --slow-ms 800        # 延迟响应
#python slave_sim.py --port COM10 --split              # 半包发送
"""

import argparse
import math
import random
import sys
import time

try:
    import serial  # pip install pyserial
except ImportError:  # 仅做协议级测试时不需要 pyserial
    serial = None

# Modbus 单帧数量上限
MAX_READ_REGISTERS = 125
MAX_WRITE_REGISTERS = 123
MAX_READ_BITS = 2000
MAX_WRITE_BITS = 1968

# 固定 8 字节请求的功能码；0x0F / 0x10 为变长请求
FIXED_FRAME_FUNCTIONS = (0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
VARIABLE_FRAME_FUNCTIONS = (0x0F, 0x10)
SUPPORTED_FUNCTIONS = FIXED_FRAME_FUNCTIONS + VARIABLE_FRAME_FUNCTIONS

# 异常码
EXC_ILLEGAL_FUNCTION = 0x01
EXC_ILLEGAL_ADDRESS = 0x02
EXC_ILLEGAL_VALUE = 0x03


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def with_crc(body: bytes) -> bytes:
    c = crc16(body)
    return body + bytes((c & 0xFF, (c >> 8) & 0xFF))   # 低字节在前


def reg_value(address: int, t: float) -> int:
    """造 4 种有意义的信号，方便在曲线上一眼看出区别。"""
    return {
        0: int(500 + 300 * math.sin(t)),      # 正弦，会波动
        1: int(t * 10) % 1000,                # 锯齿，单调增
        2: random.randint(0, 100),            # 随机噪声
        3: 1234,                              # 常量，用来验证缩放系数
    }.get(address, 0) & 0xFFFF


class RtuRequestParser:
    """把串口字节流切成完整 RTU 请求帧；CRC 错误时逐字节重同步。"""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.dropped_bytes = 0

    def reset(self) -> None:
        self._buffer.clear()

    def feed(self, data: bytes) -> list:
        """追加原始字节，返回本次解析出的完整请求帧列表。"""
        self._buffer += data
        frames = []
        while True:
            length = self._expected_length()
            if length is None or len(self._buffer) < length:
                break
            candidate = bytes(self._buffer[:length])
            if self._crc_ok(candidate):
                frames.append(candidate)
                del self._buffer[:length]
            else:
                # 帧头错位：丢弃 1 字节后重新找帧
                self.dropped_bytes += 1
                del self._buffer[:1]
        return frames

    def _expected_length(self):
        """按功能码推算整帧长度；尚未获得判断依据时返回 None。"""
        if len(self._buffer) < 2:
            return None
        func = self._buffer[1]
        if func in VARIABLE_FRAME_FUNCTIONS:
            if len(self._buffer) < 7:
                return None
            # 从站 + 功能码 + 地址2 + 数量2 + 字节数1 + 数据N + CRC2
            return 9 + self._buffer[6]
        return 8

    @staticmethod
    def _crc_ok(frame: bytes) -> bool:
        return crc16(frame[:-2]) == (frame[-2] | (frame[-1] << 8))


class SlaveStation:
    """单个从站的数据区；未写入的寄存器返回模拟信号。"""

    def __init__(self, slave_id: int, register_count: int, coil_count: int) -> None:
        self.slave_id = slave_id
        self.register_count = register_count
        self.coil_count = coil_count
        self._holding = {}
        self._coils = {}

    def holding_value(self, address: int, t: float) -> int:
        if address in self._holding:
            return self._holding[address]
        return reg_value(address, t)

    def input_value(self, address: int, t: float) -> int:
        # 输入寄存器只读，始终给模拟信号
        return reg_value(address, t)

    def set_holding(self, address: int, value: int) -> None:
        self._holding[address] = value & 0xFFFF

    def coil(self, address: int) -> bool:
        return self._coils.get(address, False)

    def set_coil(self, address: int, state: bool) -> None:
        self._coils[address] = bool(state)


class ModbusSlaveSimulator:
    """Modbus RTU 从站集合 + 故障注入，可脱离串口做协议测试。"""

    def __init__(self, slave_ids=(1,), register_count=8, coil_count=64,
                 timeout_rate=0.0, exc_rate=0.0, bad_crc_rate=0.0,
                 slow_ms=0, split=False, quiet=False, rng=None) -> None:
        self.parser = RtuRequestParser()
        self.stations = {
            int(sid): SlaveStation(int(sid), register_count, coil_count)
            for sid in slave_ids
        }
        self.timeout_rate = timeout_rate
        self.exc_rate = exc_rate
        self.bad_crc_rate = bad_crc_rate
        self.slow_ms = slow_ms
        self.split = split
        self.quiet = quiet
        self._rng = rng if rng is not None else random.Random()
        self._start = time.time()
        self.stats = {"frames": 0, "reads": 0, "writes": 0,
                      "exceptions": 0, "timeouts": 0}

    # ---------- 字节流入口（串口与测试共用） ----------

    def feed(self, data: bytes) -> bytes:
        """喂入原始字节，返回需要回写的全部应答（无应答时为空）。"""
        return b"".join(self.feed_chunks(data))

    def feed_chunks(self, data: bytes) -> list:
        """同 feed，但按半包模式拆成多个写入块，便于串口分次发送。"""
        chunks = []
        for frame in self.parser.feed(data):
            response = self._handle_frame(frame)
            if response:
                chunks.extend(self._split_response(response))
        return chunks

    # ---------- 单帧处理 ----------

    def _handle_frame(self, frame: bytes) -> bytes:
        self.stats["frames"] += 1
        slave_id, func = frame[0], frame[1]
        station = self.stations.get(slave_id)
        if station is None:
            self._log(f"not for me, addr={slave_id}")
            return b""
        if self._rng.random() < self.timeout_rate:
            self.stats["timeouts"] += 1
            self._log("inject: NO RESPONSE (timeout)")
            return b""
        if self.slow_ms:
            time.sleep(self.slow_ms / 1000.0)
        response = self._build_response(station, frame)
        if self._rng.random() < self.bad_crc_rate:
            response = response[:-2] + b"\x00\x00"
            self._log("inject: BAD CRC")
        return response

    def _build_response(self, station: SlaveStation, frame: bytes) -> bytes:
        func = frame[1]
        if func not in SUPPORTED_FUNCTIONS:
            return self._exception(station, func, EXC_ILLEGAL_FUNCTION)
        if self._rng.random() < self.exc_rate:
            code = self._rng.choice((0x01, 0x02, 0x03, 0x04))
            return self._exception(station, func, code)
        handler = {
            0x01: self._read_bits_response,
            0x02: self._read_bits_response,
            0x03: self._read_registers_response,
            0x04: self._read_registers_response,
            0x05: self._write_single_coil_response,
            0x06: self._write_single_register_response,
            0x0F: self._write_multiple_coils_response,
            0x10: self._write_multiple_registers_response,
        }[func]
        return handler(station, frame)

    # ---------- 读 ----------

    def _read_registers_response(self, station: SlaveStation, frame: bytes) -> bytes:
        start, count = self._start_and_count(frame)
        if not 1 <= count <= MAX_READ_REGISTERS:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if start + count > station.register_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        now = time.time() - self._start
        getter = station.holding_value if frame[1] == 0x03 else station.input_value
        values = [getter(start + i, now) for i in range(count)]
        payload = bytes((count * 2,))
        payload += b"".join(value.to_bytes(2, "big") for value in values)
        self.stats["reads"] += 1
        return with_crc(bytes((station.slave_id, frame[1])) + payload)

    def _read_bits_response(self, station: SlaveStation, frame: bytes) -> bytes:
        start, count = self._start_and_count(frame)
        if not 1 <= count <= MAX_READ_BITS:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if start + count > station.coil_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        raw = bytearray((count + 7) // 8)
        for i in range(count):
            # 02 读离散输入：测试环境直接镜像线圈状态
            if station.coil(start + i):
                raw[i // 8] |= 1 << (i % 8)
        self.stats["reads"] += 1
        return with_crc(bytes((station.slave_id, frame[1], len(raw))) + bytes(raw))

    # ---------- 写 ----------

    def _write_single_coil_response(self, station: SlaveStation, frame: bytes) -> bytes:
        address, value = self._start_and_count(frame)
        if value not in (0x0000, 0xFF00):
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if address >= station.coil_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        station.set_coil(address, value == 0xFF00)
        self.stats["writes"] += 1
        return with_crc(frame[:-2])   # 写单线圈应答即回显请求

    def _write_single_register_response(self, station: SlaveStation, frame: bytes) -> bytes:
        address, value = self._start_and_count(frame)
        if address >= station.register_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        station.set_holding(address, value)
        self.stats["writes"] += 1
        return with_crc(frame[:-2])   # 写单寄存器应答即回显请求

    def _write_multiple_coils_response(self, station: SlaveStation, frame: bytes) -> bytes:
        start, count = self._start_and_count(frame)
        byte_count = frame[6]
        if not 1 <= count <= MAX_WRITE_BITS:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if byte_count != (count + 7) // 8:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if start + count > station.coil_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        data = frame[7:7 + byte_count]
        for i in range(count):
            station.set_coil(start + i, bool(data[i // 8] & (1 << (i % 8))))
        self.stats["writes"] += 1
        return self._write_multiple_ack(station, frame)

    def _write_multiple_registers_response(self, station: SlaveStation, frame: bytes) -> bytes:
        start, count = self._start_and_count(frame)
        byte_count = frame[6]
        if not 1 <= count <= MAX_WRITE_REGISTERS:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if byte_count != count * 2:
            return self._exception(station, frame[1], EXC_ILLEGAL_VALUE)
        if start + count > station.register_count:
            return self._exception(station, frame[1], EXC_ILLEGAL_ADDRESS)

        for i in range(count):
            high, low = frame[7 + i * 2], frame[8 + i * 2]
            station.set_holding(start + i, (high << 8) | low)
        self.stats["writes"] += 1
        return self._write_multiple_ack(station, frame)

    # ---------- 公共辅助 ----------

    @staticmethod
    def _start_and_count(frame: bytes):
        return ((frame[2] << 8) | frame[3]), ((frame[4] << 8) | frame[5])

    @staticmethod
    def _write_multiple_ack(station: SlaveStation, frame: bytes) -> bytes:
        # 应答：从站 + 功能码 + 起始地址 + 数量
        return with_crc(bytes((station.slave_id, frame[1])) + frame[2:6])

    def _exception(self, station: SlaveStation, func: int, code: int) -> bytes:
        self.stats["exceptions"] += 1
        self._log(f"exception 0x{code:02X} func=0x{func:02X}")
        return with_crc(bytes((station.slave_id, func | 0x80, code)))

    def _split_response(self, response: bytes) -> list:
        if not self.split:
            return [response]
        half = max(1, len(response) // 2)
        self._log("inject: SPLIT (half packet)")
        return [response[:half], response[half:]]

    def _log(self, message: str) -> None:
        if not self.quiet:
            print(f"[sim] {message}")

    # ---------- 串口主循环 ----------

    def run(self, transport) -> None:
        slaves = sorted(self.stations)
        self._log(f"listening slaves={slaves} regs={self.stations[slaves[0]].register_count}")
        try:
            while True:
                data = transport.read(256)
                if not data:
                    continue
                for chunk in self.feed_chunks(data):
                    transport.write(chunk)
                    transport.flush()
                    if self.split:
                        time.sleep(0.02)
        except KeyboardInterrupt:
            self._log("stopped by user")
        finally:
            self.print_stats()
            transport.close()

    def print_stats(self) -> None:
        stats = self.stats
        print(f"[sim] frames={stats['frames']} reads={stats['reads']} "
              f"writes={stats['writes']} exceptions={stats['exceptions']} "
              f"timeouts={stats['timeouts']} dropped={self.parser.dropped_bytes}",
              file=sys.stderr)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Modbus RTU 从站模拟器")
    parser.add_argument("--port", help="串口名，如 COM10（协议级测试可不填）")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--slave", type=int, default=1, help="单从站地址（兼容旧参数）")
    parser.add_argument("--slave-ids", default="", help="多从站地址，逗号分隔，如 1,2,3")
    parser.add_argument("--regs", type=int, default=8, help="每个从站的保持寄存器数量")
    parser.add_argument("--coils", type=int, default=64, help="每个从站的线圈数量")
    parser.add_argument("--timeout-rate", type=float, default=0.0, help="不响应的概率 0~1")
    parser.add_argument("--exc-rate", type=float, default=0.0, help="返回异常码的概率 0~1")
    parser.add_argument("--bad-crc-rate", type=float, default=0.0, help="返回错误 CRC 的概率")
    parser.add_argument("--slow-ms", type=int, default=0, help="延迟响应毫秒数")
    parser.add_argument("--split", action="store_true", help="把响应拆成两半发（造半包）")
    parser.add_argument("--quiet", action="store_true", help="不打印每帧日志")
    return parser


def parse_slave_ids(args: argparse.Namespace) -> list:
    if args.slave_ids.strip():
        slave_ids = [int(part) for part in args.slave_ids.split(",") if part.strip()]
        if slave_ids:
            return slave_ids
    return [args.slave]


def main() -> int:
    args = build_arg_parser().parse_args()
    simulator = ModbusSlaveSimulator(
        slave_ids=parse_slave_ids(args),
        register_count=args.regs,
        coil_count=args.coils,
        timeout_rate=args.timeout_rate,
        exc_rate=args.exc_rate,
        bad_crc_rate=args.bad_crc_rate,
        slow_ms=args.slow_ms,
        split=args.split,
        quiet=args.quiet,
    )
    if not args.port:
        print("[sim] --port is required when running against a serial port",
              file=sys.stderr)
        return 2
    if serial is None:
        print("[sim] pyserial is required: pip install pyserial", file=sys.stderr)
        return 2
    transport = serial.Serial(args.port, args.baud, timeout=0.05)
    simulator.run(transport)
    return 0


if __name__ == "__main__":
    sys.exit(main())
