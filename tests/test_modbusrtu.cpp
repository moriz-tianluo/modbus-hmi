#include <QtTest>

#include "modbusrtu.h"

// ModbusRtu 协议层单元测试：CRC、组帧、应答解析、异常码与错误分支。
class TestModbusRtu : public QObject
{
    Q_OBJECT

private slots:
    void crc16MatchesKnownVector();
    void buildRequestMatchesKnownFrame();
    void parseHoldingRegisters();
    void parseInputRegisters();
    void parseBitResponse();
    void parseExceptionResponse();
    void parseCrcError();
    void parseShortFrame();

private:
    // 把数据域补上 CRC，拼成一帧完整应答
    static QByteArray withCrc(const QByteArray &body);
};

QByteArray TestModbusRtu::withCrc(const QByteArray &body)
{
    QByteArray frame = body;
    const quint16 crc = ModbusRtu::crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

void TestModbusRtu::crc16MatchesKnownVector()
{
    // Modbus 经典示例：01 03 00 00 00 0A → CRC = 0xCDC5（低字节 C5 在前）
    const QByteArray request = QByteArray::fromHex("01030000000A");
    QCOMPARE(ModbusRtu::crc16(request), static_cast<quint16>(0xCDC5));
}

void TestModbusRtu::buildRequestMatchesKnownFrame()
{
    const QByteArray frame = ModbusRtu::buildRequest(0x01, 0x03, 0x0000, 0x000A);
    QCOMPARE(frame, QByteArray::fromHex("01030000000AC5CD"));
    QCOMPARE(frame.size(), 8);
}

void TestModbusRtu::parseHoldingRegisters()
{
    // 01 03 04 00 0A 00 14 → 两个寄存器 10、20
    const QByteArray frame = withCrc(QByteArray::fromHex("010304000A0014"));
    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(ModbusRtu::parseResponse(frame, &result, &error));
    QVERIFY(error == ModbusError::NoError);
    QCOMPARE(result.slaveId, static_cast<quint8>(0x01));
    QCOMPARE(result.functionCode, static_cast<quint8>(0x03));
    QCOMPARE(result.registers.size(), 2);
    QCOMPARE(result.registers.at(0), static_cast<quint16>(10));
    QCOMPARE(result.registers.at(1), static_cast<quint16>(20));
    QVERIFY(result.bits.isEmpty());
}

void TestModbusRtu::parseInputRegisters()
{
    // 01 04 02 FF F6 → 单个有符号寄存器 -10 的原始值
    const QByteArray frame = withCrc(QByteArray::fromHex("010402FFF6"));
    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(ModbusRtu::parseResponse(frame, &result, &error));
    QCOMPARE(result.functionCode, static_cast<quint8>(0x04));
    QCOMPARE(result.registers.size(), 1);
    QCOMPARE(result.registers.at(0), static_cast<quint16>(0xFFF6));
}

void TestModbusRtu::parseBitResponse()
{
    // 01 01 01 05 → 8 个离散位，LSB 在前：1,0,1,0,0,0,0,0
    const QByteArray frame = withCrc(QByteArray::fromHex("01010105"));
    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(ModbusRtu::parseResponse(frame, &result, &error));
    QCOMPARE(result.bits.size(), 8);
    QVERIFY(result.bits.at(0));
    QVERIFY(!result.bits.at(1));
    QVERIFY(result.bits.at(2));
    QVERIFY(!result.bits.at(3));
}

void TestModbusRtu::parseExceptionResponse()
{
    // 01 83 02 → 读保持寄存器返回异常码 0x02（非法数据地址）
    const QByteArray frame = withCrc(QByteArray::fromHex("018302"));
    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(!ModbusRtu::parseResponse(frame, &result, &error));
    QVERIFY(error == ModbusError::ExceptionCode);
    QCOMPARE(result.exceptionCode, static_cast<quint8>(0x02));
    QVERIFY(!ModbusRtu::exceptionString(result.exceptionCode).isEmpty());
}

void TestModbusRtu::parseCrcError()
{
    QByteArray frame = withCrc(QByteArray::fromHex("010304000A0014"));
    frame[frame.size() - 1] = static_cast<char>(frame.at(frame.size() - 1) ^ 0xFF);

    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(!ModbusRtu::parseResponse(frame, &result, &error));
    QVERIFY(error == ModbusError::CrcError);
}

void TestModbusRtu::parseShortFrame()
{
    ModbusFrame result;
    ModbusError error = ModbusError::NoError;

    QVERIFY(!ModbusRtu::parseResponse(QByteArray::fromHex("0103"), &result, &error));
    QVERIFY(error == ModbusError::InvalidFrame);
    QVERIFY(!ModbusRtu::errorString(error).isEmpty());
}

QTEST_GUILESS_MAIN(TestModbusRtu)

#include "test_modbusrtu.moc"
