#include <QtTest>

#include <QSignalSpy>

#include "modbusrtu.h"
#include "serialporthelper.h"

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
    // 写请求组帧（05 / 06 / 0F / 10）
    void buildWriteFrames();
    // 写应答回显校验：一致 / 数值不一致 / 长度异常
    void writeEchoValidation();
    // 写入地址越界时拒绝下发
    void writeAddressOverflowRejected();

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

void TestModbusRtu::buildWriteFrames()
{
    // 05 写单线圈：地址 0x0010 闭合 → 数值 0xFF00
    const QByteArray coilOn = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteSingleCoil, 0x0010, 0xFF00, QByteArray());
    QCOMPARE(coilOn, withCrc(QByteArray::fromHex("01050010FF00")));
    QCOMPARE(coilOn.size(), 8);

    // 05 写单线圈：断开 → 数值 0x0000
    const QByteArray coilOff = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteSingleCoil, 0x0010, 0x0000, QByteArray());
    QCOMPARE(coilOff, withCrc(QByteArray::fromHex("010500100000")));

    // 06 写单寄存器：地址 0x0010，值 0x0064
    const QByteArray singleRegister = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteSingleRegister, 0x0010, 0x0064, QByteArray());
    QCOMPARE(singleRegister, withCrc(QByteArray::fromHex("010600100064")));

    // 0F 写多线圈：10 个线圈，字节数 2，位图 0x4D 0x03
    const QByteArray multipleCoils = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteMultipleCoils, 0x0020, 10,
        QByteArray::fromHex("024D03"));
    QCOMPARE(multipleCoils, withCrc(QByteArray::fromHex("010F0020000A024D03")));

    // 10 写多寄存器：2 个寄存器，字节数 4，值 10、20
    const QByteArray multipleRegisters = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteMultipleRegisters, 0x0010, 2,
        QByteArray::fromHex("04000A0014"));
    QCOMPARE(multipleRegisters, withCrc(QByteArray::fromHex("01100010000204000A0014")));
}

void TestModbusRtu::writeEchoValidation()
{
    // 06 请求：回显为地址 + 值（请求第 3~6 字节）
    const QByteArray request = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteSingleRegister, 0x0010, 0x0064, QByteArray());
    const QByteArray echo = withCrc(request.left(6));

    ModbusFrame result;
    ModbusError error = ModbusError::NoError;
    QVERIFY(ModbusRtu::parseResponse(echo, &result, &error));
    QCOMPARE(result.payload, QByteArray::fromHex("00100064"));
    QVERIFY(ModbusRtu::isWriteEchoMatched(request, result.payload));

    // 数值不一致：地址同为 0x0010，但值被改成 0x0065
    QVERIFY(!ModbusRtu::isWriteEchoMatched(request, QByteArray::fromHex("00100065")));
    // 地址不一致
    QVERIFY(!ModbusRtu::isWriteEchoMatched(request, QByteArray::fromHex("00110064")));
    // 长度异常
    QVERIFY(!ModbusRtu::isWriteEchoMatched(request, QByteArray::fromHex("0010")));

    // 10 写多寄存器应答回显：地址 + 数量
    const QByteArray multiRequest = ModbusRtu::buildWriteRequest(
        0x01, ModbusFunctionCode::WriteMultipleRegisters, 0x0020, 3,
        QByteArray::fromHex("06000A0014001E"));
    QVERIFY(ModbusRtu::isWriteEchoMatched(multiRequest, QByteArray::fromHex("00200003")));
}

void TestModbusRtu::writeAddressOverflowRejected()
{
    SerialPortHelper serial;
    ModbusRtu rtu(&serial);
    QSignalSpy failedSpy(&rtu, &ModbusRtu::requestFailed);

    // 0xFFFF 起写 2 个寄存器：越过 0xFFFF，地址校验阶段直接拒绝
    QVERIFY(!rtu.writeMultipleRegisters(0xFFFF, {1, 2}));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).value<ModbusError>(), ModbusError::IllegalDataAddress);

    // 边界合法（0xFFFF 单个）：通过地址校验，因串口未打开而失败
    failedSpy.clear();
    QVERIFY(!rtu.writeSingleRegister(0xFFFF, 0x1234));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).value<ModbusError>(), ModbusError::SerialError);
}

QTEST_GUILESS_MAIN(TestModbusRtu)

#include "test_modbusrtu.moc"
