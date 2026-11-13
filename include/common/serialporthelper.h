#ifndef SERIALPORTHELPER_H
#define SERIALPORTHELPER_H

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QStringList>

class QTimer;

// 串口参数集合
struct SerialPortConfig
{
    QString portName;                                       // 端口名，如 COM9
    qint32 baudRate = 9600;                                 // 波特率
    QSerialPort::DataBits dataBits = QSerialPort::Data8;    // 数据位
    QSerialPort::Parity parity = QSerialPort::NoParity;     // 校验位
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;  // 停止位
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl; // 流控
};

// 串口底层通信：打开/关闭、异步接收、3.5 字符超时判帧、参数配置、断线重连、错误上报。
// 注意：QSerialPort 必须在创建它的线程中使用，跨线程请先 moveToThread 再使用本类。
class SerialPortHelper : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortHelper(QObject *parent = nullptr);
    ~SerialPortHelper() override;

    // 枚举当前系统可用串口名，返回值可直接喂给 QComboBox::addItems
    static QStringList availablePorts();

    // 按 config 打开串口；已打开时先 close 再重开。返回值仅反映 QSerialPort::open 立即结果，
    // 异步拔出/插入导致的断线/重连走 portClosed / portOpened 信号
    bool open(const SerialPortConfig &config);
    // 关闭串口，同时停止判帧定时器和重连定时器
    void close();
    bool isOpen() const;

    // 发送数据；返回 false 时通常是串口未打开或 writeBuffer 已满
    bool send(const QByteArray &data);

    SerialPortConfig config() const;              // 最近一次 open 成功使用的配置快照
    QSerialPort::SerialPortError lastError() const; // 最近一次串口错误类型
    QString errorString() const;                  // lastError 对应的可读描述

    // 3.5 字符判帧间隔（毫秒）。<=0 时按波特率自动计算：
    //   位时间 × (1+数据位+校验位+停止位) × 3.5，>19200 波特率固定 2ms
    int frameInterval() const;
    void setFrameInterval(int milliseconds);

    // 断线自动重连开关；默认开启，间隔 2000ms、最多重试 10 次
    void setAutoReconnect(bool enabled);
    bool autoReconnect() const;
    void setReconnectInterval(int milliseconds);
    bool isReconnecting() const;

signals:
    // open 成功后发出，或自动重连成功后补发
    void portOpened(const QString &portName);
    // 串口关闭（手动 close 或异常断开）后发出；异常断开且 autoReconnect 开启时，
    // 紧接着会进入重连状态（reconnectStateChanged(true, n)）
    void portClosed(const QString &portName);
    // 一帧完整数据到达。帧间隔由 frameInterval 决定，Modbus 下建议 >= 3.5 字符时间
    void frameReceived(const QByteArray &frame);
    void bytesWritten(qint64 bytes);
    // 通信日志：outgoing=true 为发送帧，false 为接收帧；timestampMs 为本地毫秒时间戳
    void frameLogged(bool outgoing, const QByteArray &frame, qint64 timestampMs);
    // 所有串口错误统一在这里上报；NoError 不会触发
    void errorOccurred(QSerialPort::SerialPortError error, const QString &message);
    // isReconnecting 状态变化，attempt 从 0 开始计数
    void reconnectStateChanged(bool reconnecting, int attempt);

private slots:
    void onReadyRead();
    void onFrameTimeout();
    void onSerialError(QSerialPort::SerialPortError error);
    void onReconnectTimeout();

private:
    int calculateFrameInterval() const;
    void setupConnections();
    bool applyConfig();
    void startReconnect();
    void stopReconnect();
    void reportError(QSerialPort::SerialPortError error, const QString &message);

    static constexpr int kMaxReconnectAttempts = 10;

    QSerialPort *m_serial = nullptr;
    QTimer *m_frameTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    SerialPortConfig m_config;
    QByteArray m_buffer;
    int m_frameInterval = 0;        // 0 表示自动计算
    bool m_autoReconnect = true;
    int m_reconnectInterval = 2000;
    int m_reconnectAttempt = 0;
    bool m_reconnecting = false;
    QSerialPort::SerialPortError m_lastError = QSerialPort::NoError;
    QString m_errorString;
};

Q_DECLARE_METATYPE(QSerialPort::SerialPortError)

#endif // SERIALPORTHELPER_H
