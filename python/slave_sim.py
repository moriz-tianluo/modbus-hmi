import argparse, math, random, time
import serial  # pip install pyserial

###bash
# 1) 正常从站，4 路信号
#python slave_sim.py --port COM10 --slave 1 --regs 8

# 2) 20% 概率不响应（测超时重试）
#python slave_sim.py --port COM10 --timeout-rate 0.2

# 3) 每 5 帧返回一次异常码（测异常码解析）
#python slave_sim.py --port COM10 --exc-rate 0.2

# 4) 响应延迟 800ms（测你的超时阈值设置是否合理）
#python slave_sim.py --port COM10 --slow-ms 800

# 5) 半包（测你的接收缓冲是否会拼帧）
#python slave_sim.py --port COM10 --split
###


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


def reg_value(addr: int, t: float) -> int:
    """造 4 种有意义的信号，方便在曲线上一眼看出区别"""
    return {
        0: int(500 + 300 * math.sin(t)),      # 正弦，会波动
        1: int(t * 10) % 1000,                # 锯齿，单调增
        2: random.randint(0, 100),            # 随机噪声
        3: 1234,                              # 常量，用来验证缩放系数
    }.get(addr, 0) & 0xFFFF


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--slave", type=int, default=1)
    ap.add_argument("--regs", type=int, default=8)
    ap.add_argument("--timeout-rate", type=float, default=0.0, help="不响应的概率 0~1")
    ap.add_argument("--exc-rate", type=float, default=0.0, help="返回异常码的概率 0~1")
    ap.add_argument("--bad-crc-rate", type=float, default=0.0, help="返回错误 CRC 的概率")
    ap.add_argument("--slow-ms", type=int, default=0, help="延迟响应毫秒数")
    ap.add_argument("--split", action="store_true", help="把响应拆成两半发（造半包）")
    a = ap.parse_args()

    ser = serial.Serial(a.port, a.baud, timeout=0.05)
    print(f"[sim] {a.port} @ {a.baud} 8N1, slave={a.slave}, regs={a.regs}")
    t0 = time.time()
    buf = bytearray()

    while True:
        buf += ser.read(256)
        if len(buf) < 8:
            continue

        # 0x03/0x04 的请求恰好 8 字节，按此分帧（测试用足够）
        req, buf = bytes(buf[:8]), buf[8:]

        if crc16(req[:6]) != (req[6] | (req[7] << 8)):
            print("[sim] request CRC error, ignored")
            continue
        if req[0] != a.slave:
            print(f"[sim] not for me, addr={req[0]}")
            continue

        func = req[1]
        start = (req[2] << 8) | req[3]
        count = (req[4] << 8) | req[5]

        # --- 异常注入 ---
        if random.random() < a.timeout_rate:
            print("[sim] inject: NO RESPONSE (timeout)")
            continue
        if a.slow_ms:
            time.sleep(a.slow_ms / 1000.0)

        # --- 正常/异常响应 ---
        if func not in (0x03, 0x04):
            resp = with_crc(bytes((a.slave, func | 0x80, 0x01)))       # 非法功能
        elif count < 1 or count > 125:
            resp = with_crc(bytes((a.slave, func | 0x80, 0x03)))       # 非法数据值
        elif start + count > a.regs:
            resp = with_crc(bytes((a.slave, func | 0x80, 0x02)))       # 非法数据地址
        elif random.random() < a.exc_rate:
            code = random.choice((0x01, 0x02, 0x03, 0x04))
            print(f"[sim] inject: exception 0x{code:02X}")
            resp = with_crc(bytes((a.slave, func | 0x80, code)))
        else:
            t = time.time() - t0
            payload = bytes((count * 2,))                              # 字节数
            for i in range(count):
                v = reg_value(start + i, t)
                payload += bytes((v >> 8, v & 0xFF))                   # 大端
            resp = with_crc(bytes((a.slave, func)) + payload)
            print(f"[sim] func=0x{func:02X} start={start} count={count} -> OK")

        if random.random() < a.bad_crc_rate:
            resp = resp[:-2] + b"\x00\x00"                             # 错误的 CRC
            print("[sim] inject: BAD CRC")

        if a.split:
            half = len(resp) // 2
            ser.write(resp[:half]); ser.flush()
            time.sleep(0.02)
            ser.write(resp[half:])
            print("[sim] inject: SPLIT (half packet)")
        else:
            ser.write(resp)
        ser.flush()


if __name__ == "__main__":
    main()