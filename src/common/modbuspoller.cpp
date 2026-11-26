#include "modbuspoller.h"

#include <QTimer>

ModbusPoller::ModbusPoller(ModbusRtu *rtu, QObject *parent)
    : QObject(parent)
    , m_rtu(rtu)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &ModbusPoller::onPollTimeout);

    qRegisterMetaType<PollTask>("PollTask");
    qRegisterMetaType<QVector<PollTask>>("QVector<PollTask>");

    if (m_rtu) {
        connect(m_rtu, &ModbusRtu::registersRead, this, &ModbusPoller::onRegistersRead);
        connect(m_rtu, &ModbusRtu::requestFailed, this, &ModbusPoller::onRequestFailed);
        connect(m_rtu, &ModbusRtu::exceptionReceived, this, &ModbusPoller::onExceptionReceived);
    }
}

ModbusPoller::~ModbusPoller()
{
    stop();
}

void ModbusPoller::setTasks(const QVector<PollTask> &tasks)
{
    m_tasks = tasks;
    if (m_taskIndex >= m_tasks.size()) {
        m_taskIndex = 0;
    }
}

QVector<PollTask> ModbusPoller::tasks() const
{
    return m_tasks;
}

int ModbusPoller::taskCount() const
{
    return m_tasks.size();
}

int ModbusPoller::currentTaskIndex() const
{
    return m_taskIndex;
}

PollTask ModbusPoller::activeTask() const
{
    return m_activeTask;
}

void ModbusPoller::setSlaveId(quint8 slaveId)
{
    m_slaveId = slaveId;
    if (m_rtu) {
        m_rtu->setSlaveId(slaveId);
    }
}

quint8 ModbusPoller::slaveId() const
{
    return m_slaveId;
}

void ModbusPoller::setFunctionCode(ModbusFunctionCode code)
{
    // 采集值以寄存器形式返回，这里只接受 03/04
    if (isRegisterFunctionCode(code)) {
        m_functionCode = code;
    }
}

ModbusFunctionCode ModbusPoller::functionCode() const
{
    return m_functionCode;
}

void ModbusPoller::setStartAddress(quint16 address)
{
    m_startAddress = address;
}

quint16 ModbusPoller::startAddress() const
{
    return m_startAddress;
}

void ModbusPoller::setQuantity(quint16 quantity)
{
    m_quantity = qBound<quint16>(1, quantity, 125);
}

quint16 ModbusPoller::quantity() const
{
    return m_quantity;
}

void ModbusPoller::setInterval(int milliseconds)
{
    m_intervalMs = qMax(50, milliseconds);
}

int ModbusPoller::interval() const
{
    return m_intervalMs;
}

void ModbusPoller::setMaxRetries(int count)
{
    m_maxRetries = qMax(0, count);
}

int ModbusPoller::maxRetries() const
{
    return m_maxRetries;
}

void ModbusPoller::setRetryInterval(int milliseconds)
{
    m_retryInterval = qMax(50, milliseconds);
}

int ModbusPoller::retryInterval() const
{
    return m_retryInterval;
}

void ModbusPoller::start()
{
    if (m_running) {
        return;
    }

    m_running = true;
    m_retryCount = 0;
    m_taskIndex = 0;

    if (m_rtu) {
        m_rtu->setSlaveId(m_slaveId);
    }

    requestOnce();
}

void ModbusPoller::stop()
{
    m_running = false;
    m_state = PollerState::Idle;
    m_retryCount = 0;

    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}

bool ModbusPoller::isRunning() const
{
    return m_running;
}

PollerState ModbusPoller::state() const
{
    return m_state;
}

quint64 ModbusPoller::successCount() const
{
    return m_successCount;
}

quint64 ModbusPoller::failureCount() const
{
    return m_failureCount;
}

void ModbusPoller::onPollTimeout()
{
    if (!m_running) {
        return;
    }
    requestOnce();
}

void ModbusPoller::onRegistersRead(quint16 startAddress, const QVector<quint16> &values, qint64 rttMs)
{
    Q_UNUSED(startAddress)

    // 只处理本采集器发出并处于等待状态的请求
    if (!m_running || m_state != PollerState::Waiting) {
        return;
    }

    ++m_successCount;
    m_retryCount = 0;
    m_state = PollerState::Idle;

    emit roundCompleted(m_activeTask.slaveId, m_activeTask.startAddress, values, rttMs);
    advanceTask();
    scheduleNext(m_intervalMs);
}

void ModbusPoller::onRequestFailed(ModbusError error, const QString &message)
{
    if (!m_running || m_state != PollerState::Waiting) {
        return;
    }

    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        m_state = PollerState::Retrying;
        scheduleNext(m_retryInterval);
        return;
    }

    ++m_failureCount;
    m_retryCount = 0;
    m_state = PollerState::Idle;

    emit roundFailed(error, message);
    advanceTask();
    scheduleNext(m_intervalMs);
}

void ModbusPoller::onExceptionReceived(quint8 exceptionCode, const QString &message)
{
    Q_UNUSED(exceptionCode)

    // 只处理本采集器发出并处于等待状态的请求
    if (!m_running || m_state != PollerState::Waiting) {
        return;
    }

    // 从站明确回了异常码，本轮已确定失败，不再重试，直接计入失败并进入下一轮
    ++m_failureCount;
    m_retryCount = 0;
    m_state = PollerState::Idle;

    emit roundFailed(ModbusError::ExceptionCode, message);
    advanceTask();
    scheduleNext(m_intervalMs);
}

void ModbusPoller::requestOnce()
{
    if (!m_rtu || !m_running) {
        return;
    }

    // 防并发：上一请求未返回或协议层仍在等待应答时不重复发送
    if (m_state == PollerState::Waiting || m_rtu->isBusy()) {
        return;
    }

    const PollTask task = currentTask();
    if (task.quantity == 0) {
        scheduleNext(m_intervalMs);
        return;
    }

    // 多从站：每个任务切换前先更新协议层从站地址
    m_rtu->setSlaveId(task.slaveId);
    m_activeTask = task;

    m_state = PollerState::Waiting;
    const bool accepted = m_rtu->readRegisters(task.functionCode, task.startAddress,
                                               task.quantity);
    if (!accepted && m_state == PollerState::Waiting) {
        // 同步失败已由 onRequestFailed 处理，这里只兜底恢复状态
        m_state = PollerState::Idle;
        scheduleNext(m_intervalMs);
    }
}

PollTask ModbusPoller::currentTask() const
{
    if (!m_tasks.isEmpty()) {
        return m_tasks.at(m_taskIndex % m_tasks.size());
    }

    PollTask task;
    task.slaveId = m_slaveId;
    task.functionCode = m_functionCode;
    task.startAddress = m_startAddress;
    task.quantity = m_quantity;
    return task;
}

void ModbusPoller::advanceTask()
{
    if (m_tasks.isEmpty()) {
        return;
    }
    m_taskIndex = (m_taskIndex + 1) % m_tasks.size();
}

void ModbusPoller::scheduleNext(int milliseconds)
{
    if (!m_running) {
        return;
    }
    m_pollTimer->start(milliseconds);
}

bool ModbusPoller::isRegisterFunctionCode(ModbusFunctionCode code) const
{
    return code == ModbusFunctionCode::ReadHoldingRegisters
           || code == ModbusFunctionCode::ReadInputRegisters;
}
