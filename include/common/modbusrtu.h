#ifndef MODBUSRTU_H
#define MODBUSRTU_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>

class SerialPortHelper;
class QTimer;

// Modbus 功能码（本项目只使用读类功能码）
enum class ModbusFunctionCode : quint8 {
    ReadCoils = 0x01,             // 读线圈
    ReadDiscreteInputs = 0x02,    // 读离散输入
    ReadHoldingRegisters = 0x03,  // 读保持寄存器
    ReadInputRegisters = 0x04,    // 读输入寄存器
};

// 协议层错误
enum class ModbusError {
    NoError = 0,
    InvalidFrame,        // 帧长度/格式非法
    CrcError,            // CRC 校验失败
    SlaveMismatch,       // 从站地址不匹配
    FunctionMismatch,    // 功能码不匹配
    ByteCountMismatch,   // 字节数与声明不一致
    IllegalQuantity,     // 请求数量越界
    ExceptionCode,       // 从站返回异常码
    Timeout,             // 应答超时
    SerialError,         // 串口发送/连接异常
    Busy,                // 上一请求未完成
};

// 解析后的应答帧
struct ModbusFrame {
    quint8 slaveId = 0;
    quint8 functionCode = 0;
    QByteArray payload;             // 数据域原始字节
    QVector<quint16> registers;     // 寄存器值（功能码 03/04，已按大端合并）
    QVector<bool> bits;             // 线圈/离散输入（功能码 01/02，LSB 在前）
    quint8 exceptionCode = 0;
};

// Modbus RTU 协议解析层：组帧、CRC 校验、应答解析、超时重试。
// 依赖 SerialPortHelper 完成收发，二者需处于同一线程。
// 单例串行事务模型：同一时刻只处理一笔请求，frameReceived 到达时根据 slaveId/functionCode 匹配当前请求。
class ModbusRtu : public QObject
{
    Q_OBJECT

public:
    explicit ModbusRtu(SerialPortHelper *serial, QObject *parent = nullptr);
    ~ModbusRtu() override;

    void setSlaveId(quint8 slaveId);
    quint8 slaveId() const;

    // 单从站应答等待时间，超时后重试；至少 50ms
    void setResponseTimeout(int milliseconds);
    int responseTimeout() const;

    // 协议层重试次数（不含 Poller 层的重试）；0 表示不重试
    void setMaxRetries(int count);
    int maxRetries() const;

    // 上一笔请求是否仍在等待应答
    bool isBusy() const;

    // 发起一次异步读请求，返回 false 的场景：
    //   - serial == nullptr 或串口未打开（发 SerialError）
    //   - m_busy == true（发 Busy）
    //   - quantity 非法（发 IllegalQuantity）
    bool readRegisters(ModbusFunctionCode code, quint16 startAddress, quint16 quantity);

    // ---- 静态工具 ----
    // 标准 Modbus CRC16，多项式 0xA001，初值 0xFFFF
    static quint16 crc16(const QByteArray &data);
    // 组帧：从站 + 功能码 + 起始地址(大端) + 数量(大端) + CRC(低字节在前)
    static QByteArray buildRequest(quint8 slaveId, quint8 functionCode,
                                   quint16 startAddress, quint16 quantity);
    // 解析一帧完整应答。成功时 result->registers / result->bits 填充数据，返回 true；
    // 失败时 *error 指明原因，返回 false。异常码应答也走失败路径，ExceptionCode 写入 result->exceptionCode
    static bool parseResponse(const QByteArray &frame, ModbusFrame *result, ModbusError *error);
    // 协议层错误码 → 可读描述（使用 Language 国际化）
    static QString errorString(ModbusError error);
    // Modbus 异常码（0x01~0x0B）→ 可读描述
    static QString exceptionString(quint8 exceptionCode);

signals:
    // 成功应答：startAddress 是请求时的起始地址，values 寄存器值按请求顺序排列
    void registersRead(quint16 startAddress, const QVector<quint16> &values, qint64 rttMs);
    void bitsRead(quint16 startAddress, const QVector<bool> &values, qint64 rttMs);
    // 一笔请求最终失败（协议层重试已用尽，或配置非法）
    void requestFailed(ModbusError error, const QString &message);
    // 从站返回 Modbus 异常码应答
    void exceptionReceived(quint8 exceptionCode, const QString &message);

private slots:
    void onFrameReceived(const QByteArray &frame);
    void onResponseTimeout();
    void onPortClosed(const QString &portName);

private:
    bool validateQuantity(ModbusFunctionCode code, quint16 quantity) const;
    void startTransaction(const QByteArray &request);
    void handleFrame(const QByteArray &frame, qint64 rttMs);
    void retryOrFail(ModbusError error, const QString &message);
    void finishTransaction();

    static constexpr int kMaxCoilsPerRead = 2000;
    static constexpr int kMaxRegistersPerRead = 125;

    SerialPortHelper *m_serial = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QElapsedTimer m_rttTimer;

    quint8 m_slaveId = 1;
    int m_responseTimeout = 500;
    int m_maxRetries = 3;
    int m_retryCount = 0;

    bool m_busy = false;
    ModbusFunctionCode m_functionCode = ModbusFunctionCode::ReadHoldingRegisters;
    quint16 m_startAddress = 0;
    quint16 m_quantity = 0;
    QByteArray m_request;
};

Q_DECLARE_METATYPE(ModbusError)

#endif // MODBUSRTU_H
