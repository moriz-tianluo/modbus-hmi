#include <QtTest>

#include <QSignalSpy>

#include "modbuspoller.h"
#include "serialporthelper.h"

// ModbusPoller 任务调度测试：多任务轮转、从站切换、空任务回退单任务。
// 不打开串口，借“串口未打开 → 首帧失败”驱动一轮结束，验证任务推进逻辑。
class TestModbusPoller : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void taskListRotation();
    void emptyTaskListFallsBackToSingleTask();

private:
    static QVector<PollTask> makeTasks();
};

void TestModbusPoller::initTestCase()
{
    qRegisterMetaType<ModbusError>("ModbusError");
}

QVector<PollTask> TestModbusPoller::makeTasks()
{
    QVector<PollTask> tasks;
    for (int index = 0; index < 3; ++index) {
        PollTask task;
        task.slaveId = static_cast<quint8>(2 + index * 3);   // 2 / 5 / 8
        task.functionCode = ModbusFunctionCode::ReadHoldingRegisters;
        task.startAddress = static_cast<quint16>(index * 10); // 0 / 10 / 20
        task.quantity = 2;
        tasks.append(task);
    }
    return tasks;
}

void TestModbusPoller::taskListRotation()
{
    SerialPortHelper serial;
    ModbusRtu rtu(&serial);
    ModbusPoller poller(&rtu);

    poller.setInterval(50);
    poller.setMaxRetries(0);
    poller.setRetryInterval(50);
    poller.setTasks(makeTasks());

    QCOMPARE(poller.taskCount(), 3);
    QCOMPARE(poller.currentTaskIndex(), 0);
    QCOMPARE(poller.tasks().size(), 3);

    QSignalSpy failedSpy(&poller, &ModbusPoller::roundFailed);
    poller.start();

    // 串口未打开：首轮立即失败并推进到第 2 个任务；从站号尚停留在刚执行的从站 2
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(poller.currentTaskIndex(), 1);
    QCOMPARE(rtu.slaveId(), static_cast<quint8>(2));

    QVERIFY(failedSpy.wait(2000));
    QCOMPARE(poller.currentTaskIndex(), 2);
    QCOMPARE(rtu.slaveId(), static_cast<quint8>(5));

    QVERIFY(failedSpy.wait(2000));
    QCOMPARE(poller.currentTaskIndex(), 0);   // 回绕到第一个任务
    QCOMPARE(rtu.slaveId(), static_cast<quint8>(8));
    QCOMPARE(poller.failureCount(), static_cast<quint64>(3));

    poller.stop();
    QVERIFY(!poller.isRunning());
}

void TestModbusPoller::emptyTaskListFallsBackToSingleTask()
{
    SerialPortHelper serial;
    ModbusRtu rtu(&serial);
    ModbusPoller poller(&rtu);

    poller.setInterval(50);
    poller.setMaxRetries(0);
    poller.setSlaveId(7);
    poller.setStartAddress(12);
    poller.setQuantity(3);
    poller.setTasks(QVector<PollTask>());

    QCOMPARE(poller.taskCount(), 0);
    QCOMPARE(poller.currentTaskIndex(), 0);

    QSignalSpy failedSpy(&poller, &ModbusPoller::roundFailed);
    poller.start();

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(rtu.slaveId(), static_cast<quint8>(7));
    QCOMPARE(poller.currentTaskIndex(), 0);

    poller.stop();
}

QTEST_GUILESS_MAIN(TestModbusPoller)

#include "test_modbuspoller.moc"
