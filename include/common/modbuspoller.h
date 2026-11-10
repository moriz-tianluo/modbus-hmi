#ifndef MODBUSPOLLER_H
#define MODBUSPOLLER_H

#include "modbusrtu.h"

#include <QObject>
#include <QVector>

class QTimer;

// 轮询状态
enum class PollerState {
    Idle = 0,   // 空闲，等待下一个采集周期
    Waiting,    // 已发出请求，等待应答
    Retrying,   // 本次采集失败，等待短延时后重试
};

// 采集轮询器：按固定周期发起 Modbus 读请求，管理轮询状态并防止重复请求。
// 依赖 ModbusRtu（进而依赖 SerialPortHelper），三者需在同一线程。
// 周期触发 → requestOnce → ModbusRtu::readRegisters；本轮结束（成功/失败）后按 intervalMs 重排下一轮。
class ModbusPoller : public QObject
{
    Q_OBJECT

public:
    explicit ModbusPoller(ModbusRtu *rtu, QObject *parent = nullptr);
    ~ModbusPoller() override;

    // ---- 采集配置 ----
    // 启动时会同步下发到 ModbusRtu；运行中修改会在下一轮生效
    void setSlaveId(quint8 slaveId);
    quint8 slaveId() const;

    // 仅接受 ReadHoldingRegisters(03) / ReadInputRegisters(04)
    void setFunctionCode(ModbusFunctionCode code);
    ModbusFunctionCode functionCode() const;

    void setStartAddress(quint16 address);
    quint16 startAddress() const;

    // 读取长度（寄存器个数，1~125），越界会被自动截断
    void setQuantity(quint16 quantity);
    quint16 quantity() const;

    // 采集周期（毫秒）。最小 50ms，低于会被强制提升
    void setInterval(int milliseconds);
    int interval() const;

    // Poller 层的单轮重试次数（叠加在 ModbusRtu 自身重试之上，总重试 = poller × rtu）
    void setMaxRetries(int count);
    int maxRetries() const;
    // Poller 层每次重试间的等待时间
    void setRetryInterval(int milliseconds);
    int retryInterval() const;

    // ---- 运行控制 ----
    // 立即发起首轮请求并进入 Running 状态；串口未打开时首帧会失败并以 pollFailed 上报
    void start();
    // 停止周期调度，Running = false，状态回 Idle
    void stop();
    bool isRunning() const;
    PollerState state() const;

    // ---- 统计 ----
    quint64 successCount() const;
    quint64 failureCount() const;

signals:
    // 一轮采集完成，返回寄存器值；startAddress 可由外层查询 Poller::startAddress()
    void roundCompleted(const QVector<quint16> &values, qint64 rttMs);
    // 一轮采集最终失败（Poller 层与 ModbusRtu 层重试均已用尽）
    void roundFailed(ModbusError error, const QString &message);

private slots:
    void onPollTimeout();
    void onRegistersRead(quint16 startAddress, const QVector<quint16> &values, qint64 rttMs);
    void onRequestFailed(ModbusError error, const QString &message);

private:
    // 发起一次读取。前置校验：state == Idle 且 !rtu->isBusy()，否则跳过
    void requestOnce();
    // 按毫秒数安排下一次定时器触发（使用单次 QTimer::singleShot）
    void scheduleNext(int milliseconds);
    bool isRegisterFunctionCode(ModbusFunctionCode code) const;

    ModbusRtu *m_rtu = nullptr;
    QTimer *m_pollTimer = nullptr;

    quint8 m_slaveId = 1;
    ModbusFunctionCode m_functionCode = ModbusFunctionCode::ReadHoldingRegisters;
    quint16 m_startAddress = 0;
    quint16 m_quantity = 4;
    int m_intervalMs = 500;
    int m_maxRetries = 2;
    int m_retryInterval = 200;
    int m_retryCount = 0;

    bool m_running = false;
    PollerState m_state = PollerState::Idle;
    quint64 m_successCount = 0;
    quint64 m_failureCount = 0;
};

#endif // MODBUSPOLLER_H
