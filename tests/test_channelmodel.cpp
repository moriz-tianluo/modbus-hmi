#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>

#include "channelmodel.h"

// ChannelModel 单元测试：工程值换算、量程百分比、地址连续性、JSON 往返。
class TestChannelModel : public QObject
{
    Q_OBJECT

private slots:
    void valueConversion();
    void signedValueConversion();
    void percentClamps();
    void continuityValidation();
    void enabledAddressRange();
    void jsonRoundTrip();
    void jsonAcceptsGapAndSlave();
    void buildPollTasksSegments();
    void buildPollTasksSplitsByFunctionCode();
    void sameAddressDifferentFunctionAllowed();
    void channelIndexesInRangeByFunction();
    void updateRawValuesSkipsDisabled();

private:
    static ChannelItem makeChannel(quint16 address, bool enabled = true);
};

ChannelItem TestChannelModel::makeChannel(quint16 address, bool enabled)
{
    ChannelItem item;
    item.name = QStringLiteral("CH%1").arg(address);
    item.unit = QStringLiteral("u");
    item.registerAddress = address;
    item.scale = 0.1;
    item.rangeMin = 0.0;
    item.rangeMax = 100.0;
    item.decimals = 2;
    item.enabled = enabled;
    return item;
}

void TestChannelModel::valueConversion()
{
    ChannelItem item = makeChannel(0);
    item.rawValue = 812;
    QVERIFY(qAbs(item.value() - 81.2) < 1e-9);
}

void TestChannelModel::signedValueConversion()
{
    ChannelItem item = makeChannel(0);
    item.dataType = ChannelDataType::Signed16;
    item.rawValue = static_cast<quint16>(0xFFF6);  // -10
    QVERIFY(qAbs(item.value() + 1.0) < 1e-9);
}

void TestChannelModel::percentClamps()
{
    ChannelItem item = makeChannel(0);
    item.rawValue = 500;   // 50.0 → 50%
    QVERIFY(qAbs(item.percent() - 50.0) < 1e-9);

    item.rawValue = 2000;  // 200.0 → 钳位到 100%
    QVERIFY(qAbs(item.percent() - 100.0) < 1e-9);

    item.rangeMax = item.rangeMin;
    QVERIFY(qAbs(item.percent()) < 1e-9);
}

void TestChannelModel::continuityValidation()
{
    ChannelModel model;
    model.setChannels({makeChannel(0), makeChannel(1), makeChannel(2)});

    QString error;
    QVERIFY(model.validateAddressContinuity(&error));
    QVERIFY(error.isEmpty());

    // 地址断档 0,1,5 已支持多段轮询，不再视为错误
    model.setChannels({makeChannel(0), makeChannel(1), makeChannel(5)});
    QVERIFY(model.validateAddressContinuity(&error));

    // 地址重复（同一从站）仍不允许
    model.setChannels({makeChannel(0), makeChannel(0)});
    QVERIFY(!model.validateAddressContinuity());

    // 少于两个启用通道视为通过
    model.setChannels({makeChannel(0)});
    QVERIFY(model.validateAddressContinuity());
}

void TestChannelModel::enabledAddressRange()
{
    ChannelModel model;
    model.setChannels({makeChannel(0, true), makeChannel(1, false),
                       makeChannel(2, true), makeChannel(3, true)});

    quint16 start = 0;
    quint16 quantity = 0;
    QVERIFY(model.enabledAddressRange(start, quantity));
    QCOMPARE(start, static_cast<quint16>(0));
    QCOMPARE(quantity, static_cast<quint16>(4));
    QCOMPARE(model.enabledCount(), 3);
}

void TestChannelModel::jsonRoundTrip()
{
    ChannelModel source;
    source.setBufferSize(1234);
    ChannelItem first = makeChannel(0);
    first.name = QStringLiteral("温度");
    first.alarmEnabled = true;
    first.alarmHigh = 80.0;
    ChannelItem second = makeChannel(1);
    second.name = QStringLiteral("压力");
    second.dataType = ChannelDataType::Signed16;
    source.setChannels({first, second});

    const QByteArray json = source.toJsonBytes();

    ChannelModel restored;
    QString error;
    QVERIFY2(restored.fromJsonBytes(json, &error), qPrintable(error));
    QCOMPARE(restored.channelCount(), 2);
    QCOMPARE(restored.bufferSize(), 1234);
    QCOMPARE(restored.channelAt(0).name, QStringLiteral("温度"));
    QCOMPARE(restored.channelAt(0).alarmHigh, 80.0);
    QCOMPARE(restored.channelAt(1).dataType, ChannelDataType::Signed16);
}

void TestChannelModel::jsonAcceptsGapAndSlave()
{
    const QByteArray json = R"({
        "bufferSize": 100,
        "channels": [
            {"name": "A", "slave": 1, "register": 0},
            {"name": "B", "slave": 1, "register": 3},
            {"name": "C", "slave": 2, "register": 0}
        ]
    })";

    ChannelModel model;
    QString error;
    QVERIFY2(model.fromJsonBytes(json, &error), qPrintable(error));
    QCOMPARE(model.channelCount(), 3);
    QCOMPARE(model.channelAt(1).registerAddress, static_cast<quint16>(3));
    QCOMPARE(model.channelAt(2).slaveId, static_cast<quint8>(2));

    // 序列化后从站字段不丢失
    ChannelModel restored;
    QVERIFY(restored.fromJsonBytes(model.toJsonBytes(), &error));
    QCOMPARE(restored.channelAt(2).slaveId, static_cast<quint8>(2));
}

void TestChannelModel::buildPollTasksSegments()
{
    ChannelItem first = makeChannel(0);
    ChannelItem second = makeChannel(1);
    ChannelItem third = makeChannel(5);
    ChannelItem otherSlave = makeChannel(0);
    otherSlave.slaveId = 2;
    ChannelItem disabled = makeChannel(6, false);

    ChannelModel model;
    model.setChannels({first, second, third, otherSlave, disabled});

    const QVector<PollTask> tasks = model.buildPollTasks();
    // 从站 1 地址 0~1、5 各一段；从站 2 地址 0 一段
    QCOMPARE(tasks.size(), 3);
    QCOMPARE(tasks.at(0).slaveId, static_cast<quint8>(1));
    QCOMPARE(tasks.at(0).startAddress, static_cast<quint16>(0));
    QCOMPARE(tasks.at(0).quantity, static_cast<quint16>(2));
    QCOMPARE(tasks.at(1).slaveId, static_cast<quint8>(1));
    QCOMPARE(tasks.at(1).startAddress, static_cast<quint16>(5));
    QCOMPARE(tasks.at(1).quantity, static_cast<quint16>(1));
    QCOMPARE(tasks.at(2).slaveId, static_cast<quint8>(2));
}

void TestChannelModel::buildPollTasksSplitsByFunctionCode()
{
    ChannelItem holding = makeChannel(0);
    ChannelItem input = makeChannel(0);
    input.functionCode = ModbusFunctionCode::ReadInputRegisters;

    ChannelModel model;
    model.setChannels({holding, input});

    // 同从站同地址但功能码不同：必须拆成两个任务，不能合并成一次读取
    const QVector<PollTask> tasks = model.buildPollTasks();
    QCOMPARE(tasks.size(), 2);
    QCOMPARE(tasks.at(0).functionCode, ModbusFunctionCode::ReadHoldingRegisters);
    QCOMPARE(tasks.at(1).functionCode, ModbusFunctionCode::ReadInputRegisters);
    QCOMPARE(tasks.at(0).startAddress, static_cast<quint16>(0));
    QCOMPARE(tasks.at(1).startAddress, static_cast<quint16>(0));
    QCOMPARE(tasks.at(0).quantity, static_cast<quint16>(1));
    QCOMPARE(tasks.at(1).quantity, static_cast<quint16>(1));

    // JSON 往返保留每通道功能码
    QString error;
    ChannelModel restored;
    QVERIFY(restored.fromJsonBytes(model.toJsonBytes(), &error));
    QCOMPARE(restored.channelAt(1).functionCode, ModbusFunctionCode::ReadInputRegisters);
}

void TestChannelModel::sameAddressDifferentFunctionAllowed()
{
    ChannelItem holding = makeChannel(0);
    ChannelItem input = makeChannel(0);
    input.functionCode = ModbusFunctionCode::ReadInputRegisters;

    ChannelModel model;
    model.setChannels({holding, input});

    // 03 数据只回填 03 通道
    model.updateRawValues(1, ModbusFunctionCode::ReadHoldingRegisters, 0, {111});
    QCOMPARE(model.channelAt(0).rawValue, static_cast<quint16>(111));
    QCOMPARE(model.channelAt(1).rawValue, static_cast<quint16>(0));

    // 04 数据只回填 04 通道，互不串值
    model.updateRawValues(1, ModbusFunctionCode::ReadInputRegisters, 0, {222});
    QCOMPARE(model.channelAt(0).rawValue, static_cast<quint16>(111));
    QCOMPARE(model.channelAt(1).rawValue, static_cast<quint16>(222));
}

void TestChannelModel::channelIndexesInRangeByFunction()
{
    ChannelItem holding = makeChannel(0);
    ChannelItem input = makeChannel(0);
    input.functionCode = ModbusFunctionCode::ReadInputRegisters;
    ChannelItem nextHolding = makeChannel(1);
    ChannelItem otherSlave = makeChannel(0);
    otherSlave.slaveId = 2;
    ChannelItem disabledInput = makeChannel(0, false);
    disabledInput.functionCode = ModbusFunctionCode::ReadInputRegisters;

    ChannelModel model;
    model.setChannels({holding, input, nextHolding, otherSlave, disabledInput});

    // 03、从站 1、地址 0~1：只命中两条 03 通道
    QVector<int> indexes = model.channelIndexesInRange(
        1, ModbusFunctionCode::ReadHoldingRegisters, 0, 2);
    QCOMPARE(indexes, QVector<int>({0, 2}));

    // 04、从站 1、地址 0：只命中 04 通道，禁用通道被跳过
    indexes = model.channelIndexesInRange(
        1, ModbusFunctionCode::ReadInputRegisters, 0, 1);
    QCOMPARE(indexes, QVector<int>({1}));

    // 数量为 0 / 区间无覆盖时返回空
    QVERIFY(model.channelIndexesInRange(
        1, ModbusFunctionCode::ReadHoldingRegisters, 0, 0).isEmpty());
    QVERIFY(model.channelIndexesInRange(
        1, ModbusFunctionCode::ReadHoldingRegisters, 10, 2).isEmpty());
}

void TestChannelModel::updateRawValuesSkipsDisabled()
{
    ChannelModel model;
    ChannelItem enabled = makeChannel(0, true);
    ChannelItem disabled = makeChannel(1, false);
    model.setChannels({enabled, disabled});

    model.updateRawValues(0, {100, 200});
    QCOMPARE(model.channelAt(0).rawValue, static_cast<quint16>(100));
    QCOMPARE(model.channelAt(1).rawValue, static_cast<quint16>(0));
}

QTEST_GUILESS_MAIN(TestChannelModel)

#include "test_channelmodel.moc"
