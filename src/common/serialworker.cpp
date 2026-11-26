#include "serialworker.h"
#include "Language.h"

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , m_serial(new SerialPortHelper(this))
    , m_rtu(new ModbusRtu(m_serial, this))
    , m_poller(new ModbusPoller(m_rtu, this))
{
    // 跨线程队列连接需要注册自定义参数类型
    qRegisterMetaType<SerialPortConfig>("SerialPortConfig");
    qRegisterMetaType<ModbusFunctionCode>("ModbusFunctionCode");
    qRegisterMetaType<PollTask>("PollTask");
    qRegisterMetaType<QVector<PollTask>>("QVector<PollTask>");

    setupConnections();
}

SerialWorker::~SerialWorker()
{
    // 子对象随本对象销毁；本对象应由 QThread::finished -> deleteLater 在其所属线程内销毁
    if (m_polling) {
        m_poller->stop();
    }
}

bool SerialWorker::isPortOpen() const
{
    return m_portOpen;
}

bool SerialWorker::isPolling() const
{
    return m_polling;
}

void SerialWorker::openPort(const SerialPortConfig &config)
{
    if (m_portOpen) {
        stopPolling();
        m_serial->close();
    }
    // 手动打开视为新会话，不恢复历史采集状态
    m_resumePolling = false;
    // 打开失败会通过 errorOccurred 上报
    m_serial->open(config);
}

void SerialWorker::closePort()
{
    stopPolling();
    m_resumePolling = false;
    m_serial->close();
}

void SerialWorker::startPolling(quint8 slaveId, ModbusFunctionCode code,
                                quint16 startAddress, quint16 quantity, int intervalMs)
{
    if (!m_portOpen) {
        emit pollFailed(ModbusError::SerialError, LANGUAGE("LG_modbus_serialNotOpen"));
        return;
    }

    // 单任务模式：清空任务列表，Poller 回退到 slaveId/start/quantity 配置
    m_poller->setTasks(QVector<PollTask>());
    m_poller->setSlaveId(slaveId);
    m_poller->setFunctionCode(code);
    m_poller->setStartAddress(startAddress);
    m_poller->setQuantity(quantity);
    m_poller->setInterval(intervalMs);
    m_startAddress = startAddress;

    m_poller->start();
    setPolling(true);
}

void SerialWorker::startPollingTasks(const QVector<PollTask> &tasks, int intervalMs)
{
    if (!m_portOpen) {
        emit pollFailed(ModbusError::SerialError, LANGUAGE("LG_modbus_serialNotOpen"));
        return;
    }
    if (tasks.isEmpty()) {
        emit pollFailed(ModbusError::IllegalQuantity, LANGUAGE("LG_modbus_noPollTask"));
        return;
    }

    m_poller->setTasks(tasks);
    m_poller->setInterval(intervalMs);
    m_poller->start();
    setPolling(true);
}

void SerialWorker::writeSingleRegister(quint8 slaveId, quint16 address, quint16 value)
{
    beginWrite(slaveId);
    m_rtu->writeSingleRegister(address, value);
}

void SerialWorker::writeMultipleRegisters(quint8 slaveId, quint16 startAddress,
                                          const QVector<quint16> &values)
{
    beginWrite(slaveId);
    m_rtu->writeMultipleRegisters(startAddress, values);
}

void SerialWorker::writeSingleCoil(quint8 slaveId, quint16 address, bool on)
{
    beginWrite(slaveId);
    m_rtu->writeSingleCoil(address, on);
}

void SerialWorker::writeMultipleCoils(quint8 slaveId, quint16 startAddress,
                                      const QVector<bool> &values)
{
    beginWrite(slaveId);
    m_rtu->writeMultipleCoils(startAddress, values);
}

void SerialWorker::beginWrite(quint8 slaveId)
{
    m_writePending = true;
    m_resumeAfterWrite = m_polling;
    if (m_polling) {
        // 写事务期间暂停轮询，保证同一时刻只有一笔事务占用串口
        m_poller->stop();
        setPolling(false);
    }
    m_rtu->setSlaveId(slaveId);
}

void SerialWorker::endWrite()
{
    m_writePending = false;

    if (m_resumeAfterWrite && m_portOpen) {
        m_resumeAfterWrite = false;
        m_poller->start();
        setPolling(true);
    }
    m_resumeAfterWrite = false;
}

void SerialWorker::stopPolling()
{
    if (!m_polling) {
        return;
    }
    m_poller->stop();
    setPolling(false);
}

void SerialWorker::updatePollingConfig(quint8 slaveId, quint16 startAddress, quint16 quantity)
{
    m_poller->setSlaveId(slaveId);
    m_poller->setStartAddress(startAddress);
    m_poller->setQuantity(quantity);
    m_startAddress = startAddress;
}

void SerialWorker::applyCommunicationOptions(int responseTimeoutMs, int retryCount)
{
    // 兜底约束，避免 UI 传入手误参数导致长时间无响应
    const int timeoutMs = qBound(50, responseTimeoutMs, 10000);
    const int retries = qBound(0, retryCount, 10);

    m_rtu->setResponseTimeout(timeoutMs);
    m_rtu->setMaxRetries(retries);

    // 帧级重试已覆盖界面「重试」语义，轮询层不再叠加，避免实际尝试次数被放大
    m_poller->setMaxRetries(0);
}

void SerialWorker::onPortOpened(const QString &portName)
{
    m_portOpen = true;
    emit portOpened(portName);

    if (m_resumePolling) {
        m_resumePolling = false;
        // 重连成功，Poller 仍保留原配置，直接恢复采集
        m_poller->start();
        setPolling(true);
    }
}

void SerialWorker::onPortClosed(const QString &portName)
{
    m_portOpen = false;

    if (m_polling) {
        // 能走到这里说明是异常断开（手动 closePort 会先 stopPolling，此处 m_polling 已为 false）
        m_resumePolling = true;
        m_poller->stop();
        setPolling(false);
    }

    emit portClosed(portName);
}

void SerialWorker::onSerialError(QSerialPort::SerialPortError error, const QString &message)
{
    emit portError(error, message);
}

void SerialWorker::onRoundCompleted(quint8 slaveId, quint16 startAddress,
                                    const QVector<quint16> &values, qint64 rttMs)
{
    // 带上本轮的读取功能码，供上层区分同地址的 03 / 04 通道
    const quint8 functionCode = m_poller
        ? static_cast<quint8>(m_poller->activeTask().functionCode)
        : static_cast<quint8>(ModbusFunctionCode::ReadHoldingRegisters);
    emit dataUpdated(slaveId, functionCode, startAddress, values, rttMs);
}

void SerialWorker::onWriteCompleted(quint8 functionCode, quint16 startAddress,
                                    quint16 quantity)
{
    endWrite();
    emit writeSucceeded(functionCode, startAddress, quantity);
}

void SerialWorker::onWriteRequestFailed(ModbusError error, const QString &message)
{
    // 只有写事务期间才由本槽接管，普通轮询失败仍走 pollFailed
    if (!m_writePending) {
        return;
    }
    endWrite();
    emit writeFailed(error, message);
}

void SerialWorker::setupConnections()
{
    connect(m_serial, &SerialPortHelper::portOpened, this, &SerialWorker::onPortOpened);
    connect(m_serial, &SerialPortHelper::portClosed, this, &SerialWorker::onPortClosed);
    connect(m_serial, &SerialPortHelper::errorOccurred, this, &SerialWorker::onSerialError);
    connect(m_serial, &SerialPortHelper::reconnectStateChanged,
            this, &SerialWorker::reconnectStateChanged);
    // 通信日志帧直接透传（跨线程由接收端自动队列化）
    connect(m_serial, &SerialPortHelper::frameLogged, this, &SerialWorker::frameLogged);

    connect(m_poller, &ModbusPoller::roundCompleted, this, &SerialWorker::onRoundCompleted);
    // 信号到信号的直接转发
    connect(m_poller, &ModbusPoller::roundFailed, this, &SerialWorker::pollFailed);

    // 写事务结果：写操作会让 Poller 暂时停止，因此失败/成功都要在这里恢复采集
    connect(m_rtu, &ModbusRtu::writeCompleted, this, &SerialWorker::onWriteCompleted);
    connect(m_rtu, &ModbusRtu::requestFailed, this, &SerialWorker::onWriteRequestFailed);
}

void SerialWorker::setPolling(bool running)
{
    if (m_polling == running) {
        return;
    }
    m_polling = running;
    emit pollingStateChanged(m_polling);
}
