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

    m_poller->setSlaveId(slaveId);
    m_poller->setFunctionCode(code);
    m_poller->setStartAddress(startAddress);
    m_poller->setQuantity(quantity);
    m_poller->setInterval(intervalMs);
    m_startAddress = startAddress;

    m_poller->start();
    setPolling(true);
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

void SerialWorker::onRoundCompleted(const QVector<quint16> &values, qint64 rttMs)
{
    emit dataUpdated(m_startAddress, values, rttMs);
}

void SerialWorker::setupConnections()
{
    connect(m_serial, &SerialPortHelper::portOpened, this, &SerialWorker::onPortOpened);
    connect(m_serial, &SerialPortHelper::portClosed, this, &SerialWorker::onPortClosed);
    connect(m_serial, &SerialPortHelper::errorOccurred, this, &SerialWorker::onSerialError);
    connect(m_serial, &SerialPortHelper::reconnectStateChanged,
            this, &SerialWorker::reconnectStateChanged);

    connect(m_poller, &ModbusPoller::roundCompleted, this, &SerialWorker::onRoundCompleted);
    // 信号到信号的直接转发
    connect(m_poller, &ModbusPoller::roundFailed, this, &SerialWorker::pollFailed);
}

void SerialWorker::setPolling(bool running)
{
    if (m_polling == running) {
        return;
    }
    m_polling = running;
    emit pollingStateChanged(m_polling);
}
