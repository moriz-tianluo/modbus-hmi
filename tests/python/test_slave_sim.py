"""slave_sim.py 协议级单元测试：无需串口硬件与 com0com。

运行：
    python -m unittest discover -s tests/python -p "test_*.py" -v
"""

import random
import sys
import unittest
from pathlib import Path

_TESTS_DIR = Path(__file__).resolve().parent
if str(_TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(_TESTS_DIR))

import rtu_master as mb                                    # noqa: E402
from slave_sim import ModbusSlaveSimulator, with_crc       # noqa: E402


def make_sim(**kwargs) -> ModbusSlaveSimulator:
    """使用固定随机种子，保证故障注入用例可复现。"""
    kwargs.setdefault("rng", random.Random(1234))
    return ModbusSlaveSimulator(**kwargs)


class ReadTestCase(unittest.TestCase):
    """01 / 02 / 03 / 04 读取与异常应答。"""

    def test_read_holding_registers(self):
        sim = make_sim(register_count=8)
        response = sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 4))
        self.assertTrue(mb.is_valid(response))
        self.assertEqual((response[0], response[1]), (1, mb.READ_HOLDING))
        values = mb.register_values(response)
        self.assertEqual(len(values), 4)
        self.assertEqual(values[3], 1234)          # 常量信号，验证取值通道

    def test_read_input_registers(self):
        sim = make_sim(register_count=8)
        response = sim.feed(mb.read_request(1, mb.READ_INPUT, 1, 2))
        self.assertTrue(mb.is_valid(response))
        self.assertEqual(response[1], mb.READ_INPUT)
        self.assertEqual(len(mb.register_values(response)), 2)

    def test_read_coils_after_write(self):
        sim = make_sim()
        sim.feed(mb.write_multiple_coils(1, 5, [True, False, True]))
        response = sim.feed(mb.read_request(1, mb.READ_COILS, 5, 3))
        self.assertTrue(mb.is_valid(response))
        self.assertEqual(mb.bit_values(response, 3), [True, False, True])

    def test_address_out_of_range(self):
        sim = make_sim(register_count=8)
        response = sim.feed(mb.read_request(1, mb.READ_HOLDING, 6, 4))
        self.assertEqual(mb.exception_code(response), 0x02)

    def test_zero_count_rejected(self):
        sim = make_sim()
        response = sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 0))
        self.assertEqual(mb.exception_code(response), 0x03)

    def test_illegal_function(self):
        sim = make_sim()
        # 未实现的功能码 0x2B，按 8 字节帧解析
        frame = with_crc(bytes((1, 0x2B, 0x00, 0x00, 0x00, 0x00)))
        response = sim.feed(frame)
        self.assertEqual(mb.exception_code(response), 0x01)


class WriteTestCase(unittest.TestCase):
    """05 / 06 / 0F / 10 写入、回显与读回校验。"""

    def test_write_single_register_echo_and_readback(self):
        sim = make_sim(register_count=8)
        request = mb.write_single_register(1, 5, 4321)
        response = sim.feed(request)
        self.assertEqual(response, request)        # 写单寄存器应答即回显
        readback = sim.feed(mb.read_request(1, mb.READ_HOLDING, 5, 1))
        self.assertEqual(mb.register_values(readback), [4321])

    def test_write_multiple_registers(self):
        sim = make_sim(register_count=8)
        response = sim.feed(mb.write_multiple_registers(1, 4, [11, 22, 33]))
        self.assertTrue(mb.is_valid(response))
        self.assertEqual(response[2:6], bytes((0, 4, 0, 3)))   # 起始地址 + 数量
        readback = sim.feed(mb.read_request(1, mb.READ_HOLDING, 4, 3))
        self.assertEqual(mb.register_values(readback), [11, 22, 33])

    def test_write_single_coil_echo_and_readback(self):
        sim = make_sim()
        request = mb.write_single_coil(1, 9, True)
        response = sim.feed(request)
        self.assertEqual(response, request)
        readback = sim.feed(mb.read_request(1, mb.READ_COILS, 9, 1))
        self.assertEqual(mb.bit_values(readback, 1), [True])

    def test_write_multiple_coils(self):
        sim = make_sim()
        sim.feed(mb.write_multiple_coils(1, 0, [True, True, False, True]))
        readback = sim.feed(mb.read_request(1, mb.READ_COILS, 0, 4))
        self.assertEqual(mb.bit_values(readback, 4), [True, True, False, True])

    def test_invalid_coil_value_rejected(self):
        sim = make_sim()
        frame = with_crc(bytes((1, mb.WRITE_COIL, 0x00, 0x01, 0x12, 0x34)))
        self.assertEqual(mb.exception_code(sim.feed(frame)), 0x03)

    def test_write_register_out_of_range(self):
        sim = make_sim(register_count=8)
        response = sim.feed(mb.write_single_register(1, 8, 1))
        self.assertEqual(mb.exception_code(response), 0x02)


class MultiSlaveTestCase(unittest.TestCase):
    """多从站地址路由与数据隔离。"""

    def test_route_to_matching_slave(self):
        sim = make_sim(slave_ids=(1, 2), register_count=8)
        sim.feed(mb.write_single_register(2, 0, 777))

        first = sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        second = sim.feed(mb.read_request(2, mb.READ_HOLDING, 0, 1))
        self.assertEqual(first[0], 1)
        self.assertEqual(second[0], 2)
        self.assertNotEqual(mb.register_values(first), [777])
        self.assertEqual(mb.register_values(second), [777])

    def test_unknown_slave_ignored(self):
        sim = make_sim(slave_ids=(1,))
        self.assertEqual(sim.feed(mb.read_request(9, mb.READ_HOLDING, 0, 1)), b"")


class FrameParsingTestCase(unittest.TestCase):
    """分帧、粘包、CRC 错误重同步。"""

    def test_fragmented_request(self):
        sim = make_sim()
        request = mb.read_request(1, mb.READ_HOLDING, 0, 2)
        responses = [sim.feed(bytes((byte,))) for byte in request]
        non_empty = [item for item in responses if item]
        self.assertEqual(len(non_empty), 1)
        self.assertTrue(mb.is_valid(non_empty[0]))

    def test_two_requests_in_one_buffer(self):
        sim = make_sim()
        request = mb.read_request(1, mb.READ_HOLDING, 0, 1)
        blob = sim.feed(request + request)
        self.assertEqual(len(blob), 14)            # 每条应答 7 字节
        self.assertTrue(mb.is_valid(blob[:7]))
        self.assertTrue(mb.is_valid(blob[7:]))

    def test_bad_crc_is_dropped(self):
        sim = make_sim()
        request = bytearray(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        request[-1] ^= 0xFF
        self.assertEqual(sim.feed(bytes(request)), b"")
        self.assertGreater(sim.parser.dropped_bytes, 0)


class FaultInjectionTestCase(unittest.TestCase):
    """超时、异常码、坏 CRC、半包注入。"""

    def test_timeout_injection(self):
        sim = make_sim(timeout_rate=1.0)
        self.assertEqual(sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 1)), b"")
        self.assertEqual(sim.stats["timeouts"], 1)

    def test_exception_injection(self):
        sim = make_sim(exc_rate=1.0)
        response = sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        self.assertIn(mb.exception_code(response), (0x01, 0x02, 0x03, 0x04))

    def test_bad_crc_injection(self):
        sim = make_sim(bad_crc_rate=1.0)
        response = sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        self.assertFalse(mb.is_valid(response))

    def test_split_injection(self):
        sim = make_sim(split=True)
        chunks = sim.feed_chunks(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        self.assertEqual(len(chunks), 2)
        self.assertTrue(mb.is_valid(b"".join(chunks)))

    def test_stats_counters(self):
        sim = make_sim(register_count=8)
        sim.feed(mb.read_request(1, mb.READ_HOLDING, 0, 1))
        sim.feed(mb.write_single_register(1, 4, 55))
        self.assertEqual(sim.stats["frames"], 2)
        self.assertEqual(sim.stats["reads"], 1)
        self.assertEqual(sim.stats["writes"], 1)


if __name__ == "__main__":
    unittest.main()
