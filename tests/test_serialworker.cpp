#include <QtTest>

#include <QSignalSpy>
#include <QThread>

#include "serialworker.h"

// SerialWorker 线程投递测试：验证 UI 线程通过队列调用 worker 槽时，
// 参数类型可跨线程传递、失败路径能正确回传信号（不依赖真实串口硬件）。
class TestSerialWorker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void queuedWriteWithoutPortFails();
    void queuedStartPollingWithoutPortFails();

private:
    QThread *m_thread = nullptr;
    SerialWorker *m_worker = nullptr;
};

void TestSerialWorker::initTestCase()
{
    qRegisterMetaType<ModbusError>("ModbusError");
}

void TestSerialWorker::init()
{
    m_thread = new QThread(this);
    m_worker = new SerialWorker();
    m_worker->moveToThread(m_thread);
    m_thread->start();
}

void TestSerialWorker::cleanup()
{
    m_thread->quit();
    m_thread->wait(3000);

    delete m_worker;
    m_worker = nullptr;

    delete m_thread;
    m_thread = nullptr;
}

void TestSerialWorker::queuedWriteWithoutPortFails()
{
    QSignalSpy failedSpy(m_worker, &SerialWorker::writeFailed);

    // 队列化投递到工作线程：未打开串口应回 writeFailed(SerialError)
    QMetaObject::invokeMethod(m_worker, "writeSingleRegister", Qt::QueuedConnection,
                              Q_ARG(quint8, 1), Q_ARG(quint16, 0), Q_ARG(quint16, 1));

    QVERIFY(failedSpy.wait(3000));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).value<ModbusError>(), ModbusError::SerialError);
    QVERIFY(!m_worker->isPolling());
}

void TestSerialWorker::queuedStartPollingWithoutPortFails()
{
    QVector<PollTask> tasks;
    PollTask task;
    task.slaveId = 1;
    task.functionCode = ModbusFunctionCode::ReadHoldingRegisters;
    task.startAddress = 0;
    task.quantity = 2;
    tasks.append(task);

    QSignalSpy failedSpy(m_worker, &SerialWorker::pollFailed);

    QMetaObject::invokeMethod(m_worker, "startPollingTasks", Qt::QueuedConnection,
                              Q_ARG(QVector<PollTask>, tasks), Q_ARG(int, 100));

    QVERIFY(failedSpy.wait(3000));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).value<ModbusError>(), ModbusError::SerialError);
    QVERIFY(!m_worker->isPolling());
}

QTEST_GUILESS_MAIN(TestSerialWorker)

#include "test_serialworker.moc"
