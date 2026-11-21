#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include "modbuspoller.h"
#include "serialporthelper.h"

#include <QObject>

// 串口采集工作线程对象，整体架构：
//   ┌─ SerialPortHelper (底层收发 + 重连)
//   ├─ ModbusRtu       (协议组帧/解帧 + CRC)  三者构造时一并在本对象内创建
//   └─ ModbusPoller    (周期调度 + 防并发)     随本对象 moveToThread 迁入工作线程
//
// 线程模型：本对象本身无 parent，由外部 QThread 持有，调用方显式 worker->moveToThread(workerThread)。
// 构造时对 SerialPortConfig / ModbusFunctionCode 执行 qRegisterMetaType，保障跨线程信号槽参数可队列化传输。
// UI 线程通过信号 → 本类 public slots 触发（跨线程自动转为 Qt::QueuedConnection），
// 采集结果经本类 signals 回流到 UI 线程（信号连接时 Qt 自动判定跨线程并队列化）。
//
// 重连恢复机制：
//   - 手动 openPort / closePort 会清除 m_resumePolling = false，重连后不自动恢复采集
//   - 异常断开（SerialPortHelper 发出 portClosed 时 m_polling 仍为 true）→ m_resumePolling = true
//     重连成功触发 onPortOpened 时，Poller 配置仍保留原样，直接 poller->start() 恢复采集
//   - Poller 与 ModbusRtu 均有各自的重试层，重连恢复会重新进入轮询周期，不叠加历史重试状态
class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

    // ---- 状态查询 ----
    // 以下两个方法读取的是本线程内缓存的 bool 标志；UI 线程直读时无数据竞争但语义为"上次已知状态"，
    // 精确状态变更应以 portOpened / pollingStateChanged 信号为准
    bool isPortOpen() const;
    bool isPolling() const;

public slots:
    // 打开串口；已打开时先 stopPolling + close，再按新配置打开。
    // 手动操作，失败/成功均通过信号上报，不触发自动恢复采集（m_resumePolling 清除）
    void openPort(const SerialPortConfig &config);
    // 关闭串口（内部先 stopPolling），无论当前是否在采集均调用
    void closePort();

    // 启动周期采集。首次调用前必须保证串口已打开，否则直接发 pollFailed(SerialError)。
    // 参数会下发到 ModbusPoller，Poller 配置变化在下一轮生效
    void startPolling(quint8 slaveId, ModbusFunctionCode code,
                      quint16 startAddress, quint16 quantity, int intervalMs);
    // 多从站 / 多段地址：按任务列表轮询，覆盖 startPolling 的单任务模式
    void startPollingTasks(const QVector<PollTask> &tasks, int intervalMs);
    // 停止周期采集，无论是否在采集均安全调用
    void stopPolling();

    // 采集运行中动态调整从站/起始地址/数量，下一轮 Poller 调度生效。
    // 同时更新本对象缓存的 m_startAddress（用于 dataUpdated 信号带出）
    void updatePollingConfig(quint8 slaveId, quint16 startAddress, quint16 quantity);

    // 下发串口卡片的响应超时与重试次数（UI 线程调用，跨线程队列化）。
    // 重试由 ModbusRtu 帧级承担，Poller 层不再叠加，保证界面「重试 N 次」语义唯一
    void applyCommunicationOptions(int responseTimeoutMs, int retryCount);

    // ---- 写操作（写保护在上层对话框完成，本层只负责串行下发）----
    void writeSingleRegister(quint8 slaveId, quint16 address, quint16 value);
    void writeMultipleRegisters(quint8 slaveId, quint16 startAddress,
                                const QVector<quint16> &values);
    void writeSingleCoil(quint8 slaveId, quint16 address, bool on);
    void writeMultipleCoils(quint8 slaveId, quint16 startAddress,
                            const QVector<bool> &values);

signals:
    // ---- 串口状态 ----
    void portOpened(const QString &portName);              // open 成功或自动重连成功
    void portClosed(const QString &portName);              // 手动或异常关闭
    void portError(QSerialPort::SerialPortError error, const QString &message);  // 串口层错误统一出口
    void reconnectStateChanged(bool reconnecting, int attempt);  // SerialPortHelper 的重连状态透传

    // ---- 采集结果 ----
    // 一轮采集完成。slaveId / startAddress 对应当前轮询任务，
    // values 寄存器值数组，按 Modbus 应答顺序排列
    void dataUpdated(quint8 slaveId, quint16 startAddress,
                     const QVector<quint16> &values, qint64 rttMs);
    // 写操作结果（成功时回显已校验）
    void writeSucceeded(quint8 functionCode, quint16 startAddress, quint16 quantity);
    void writeFailed(ModbusError error, const QString &message);
    // 一轮采集最终失败（Poller 层 × ModbusRtu 层重试均已用尽）
    void pollFailed(ModbusError error, const QString &message);
    // 轮询运行状态变化：running=true 表示 Poller::start() 已执行且正在调度
    void pollingStateChanged(bool running);
    // 通信日志：收发帧透传（outgoing=true 发送，false 接收）
    void frameLogged(bool outgoing, const QByteArray &frame, qint64 timestampMs);

private slots:
    void onPortOpened(const QString &portName);     // SerialPortHelper::portOpened → 更新 m_portOpen + 判断是否恢复采集
    void onPortClosed(const QString &portName);     // SerialPortHelper::portClosed → 异常断开时置 m_resumePolling = true
    void onSerialError(QSerialPort::SerialPortError error, const QString &message);
    void onRoundCompleted(quint8 slaveId, quint16 startAddress,
                          const QVector<quint16> &values, qint64 rttMs);
    void onWriteCompleted(quint8 functionCode, quint16 startAddress, quint16 quantity);
    // 写操作期间协议层失败：转成 writeFailed 并恢复采集
    void onWriteRequestFailed(ModbusError error, const QString &message);

private:
    void setupConnections();
    void setPolling(bool running);
    // 写操作前暂停轮询，保证串行事务
    void beginWrite(quint8 slaveId);
    // 写操作结束（成功或失败）：按需恢复轮询
    void endWrite();

    // ---- 内部子对象 ----
    SerialPortHelper *m_serial = nullptr;  // 底层串口通信
    ModbusRtu *m_rtu = nullptr;            // 协议组帧/解帧
    ModbusPoller *m_poller = nullptr;      // 周期轮询调度

    // ---- 本地状态缓存（供 isPortOpen/isPolling 直读）----
    bool m_portOpen = false;
    bool m_polling = false;
    // 异常断开前是否正在采集：onPortClosed 中根据 m_polling 判定是否为异常断开，
    // 异常断开 → 置 true → 重连成功后 onPortOpened 自动 poller->start() 恢复
    bool m_resumePolling = false;
    quint16 m_startAddress = 0;   // 采集起始地址缓存，dataUpdated 信号需要携带
    bool m_writePending = false;  // 是否有写事务占用了串口
    bool m_resumeAfterWrite = false;  // 写事务前是否在采集，写完需恢复
};

#endif // SERIALWORKER_H
