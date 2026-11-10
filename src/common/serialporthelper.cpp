#include "serialporthelper.h"

#include <QSerialPortInfo>
#include <QTimer>
#include <QtMath>
#include "Language.h"



SerialPortHelper::SerialPortHelper(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_frameTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");

    m_frameTimer->setSingleShot(true);
    m_reconnectTimer->setSingleShot(true);
    setupConnections();
}

SerialPortHelper::~SerialPortHelper()
{
    close();
}

void SerialPortHelper::setupConnections()
{
    connect(m_serial, &QSerialPort::readyRead, this, &SerialPortHelper::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialPortHelper::onSerialError);
    connect(m_frameTimer, &QTimer::timeout, this, &SerialPortHelper::onFrameTimeout);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SerialPortHelper::onReconnectTimeout);
}

QStringList SerialPortHelper::availablePorts()
{
    QStringList portNames;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        portNames.append(info.portName());
    }
    return portNames;
}

bool SerialPortHelper::open(const SerialPortConfig &config)
{
    if (m_serial->isOpen()) {
        close();
    }

    m_config = config;
    if (m_config.portName.isEmpty()) {
        reportError(QSerialPort::DeviceNotFoundError, LANGUAGE("LG_statusbar_noPortNameSpecified"));
        return false;
    }

    if (!applyConfig()) {
        return false;
    }

    m_reconnectAttempt = 0;
    m_buffer.clear();
    emit portOpened(m_config.portName);
    return true;
}

void SerialPortHelper::close()
{
    stopReconnect();
    m_frameTimer->stop();
    m_buffer.clear();

    if (!m_serial->isOpen()) {
        return;
    }

    const QString portName = m_serial->portName();
    m_serial->close();
    emit portClosed(portName);
}

bool SerialPortHelper::isOpen() const
{
    return m_serial->isOpen();
}

bool SerialPortHelper::send(const QByteArray &data)
{
    if (!m_serial->isOpen()) {
        reportError(QSerialPort::NotOpenError, LANGUAGE("LG_statusbar_serialNotOpen"));
        return false;
    }

    const qint64 written = m_serial->write(data);
    if (written != data.size()) {
        reportError(m_serial->error(), m_serial->errorString());
        return false;
    }

    emit bytesWritten(written);
    return true;
}

SerialPortConfig SerialPortHelper::config() const
{
    return m_config;
}

QSerialPort::SerialPortError SerialPortHelper::lastError() const
{
    return m_lastError;
}

QString SerialPortHelper::errorString() const
{
    return m_errorString;
}

int SerialPortHelper::frameInterval() const
{
    return m_frameInterval > 0 ? m_frameInterval : calculateFrameInterval();
}

void SerialPortHelper::setFrameInterval(int milliseconds)
{
    m_frameInterval = milliseconds;
}

void SerialPortHelper::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
    if (!enabled) {
        stopReconnect();
    }
}

bool SerialPortHelper::autoReconnect() const
{
    return m_autoReconnect;
}

void SerialPortHelper::setReconnectInterval(int milliseconds)
{
    m_reconnectInterval = qMax(100, milliseconds);
}

bool SerialPortHelper::isReconnecting() const
{
    return m_reconnecting;
}

void SerialPortHelper::onReadyRead()
{
    m_buffer.append(m_serial->readAll());
    if (m_buffer.isEmpty()) {
        return;
    }

    // 每收到新数据就重新计时，静默 3.5 个字符即认为一帧结束
    m_frameTimer->start(frameInterval());
}

void SerialPortHelper::onFrameTimeout()
{
    if (m_buffer.isEmpty()) {
        return;
    }

    const QByteArray frame = m_buffer;
    m_buffer.clear();
    emit frameReceived(frame);
}

void SerialPortHelper::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    reportError(error, m_serial->errorString());

    // 设备被拔出（ResourceError）时关闭端口并按需自动重连
    if (error != QSerialPort::ResourceError) {
        return;
    }

    if (m_serial->isOpen()) {
        m_serial->close();
        emit portClosed(m_config.portName);
    }
    if (m_autoReconnect) {
        startReconnect();
    }
}

void SerialPortHelper::onReconnectTimeout()
{
    if (!m_reconnecting) {
        return;
    }

    ++m_reconnectAttempt;
    if (applyConfig()) {
        m_buffer.clear();
        m_reconnecting = false;
        m_reconnectAttempt = 0;
        emit portOpened(m_config.portName);
        emit reconnectStateChanged(false, 0);
        return;
    }

    if (m_reconnectAttempt >= kMaxReconnectAttempts) {
        m_reconnecting = false;
        emit reconnectStateChanged(false, m_reconnectAttempt);
        return;
    }

    m_reconnectTimer->start(m_reconnectInterval);
}

int SerialPortHelper::calculateFrameInterval() const
{
    const int bitsPerChar = 1 + static_cast<int>(m_config.dataBits)
                            + (m_config.parity == QSerialPort::NoParity ? 0 : 1)
                            + static_cast<int>(m_config.stopBits);

    // 波特率高于 19200 时，Modbus 规范固定使用 1750us，这里取整为 2ms
    if (m_config.baudRate > 19200) {
        return 2;
    }

    const double interval = 3.5 * bitsPerChar * 1000.0 / m_config.baudRate;
    return qMax(1, qCeil(interval));
}

bool SerialPortHelper::applyConfig()
{
    m_serial->setPortName(m_config.portName);
    m_serial->setBaudRate(m_config.baudRate);
    m_serial->setDataBits(m_config.dataBits);
    m_serial->setParity(m_config.parity);
    m_serial->setStopBits(m_config.stopBits);
    m_serial->setFlowControl(m_config.flowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        reportError(m_serial->error(), m_serial->errorString());
        return false;
    }
    return true;
}

void SerialPortHelper::startReconnect()
{
    if (m_reconnecting || m_config.portName.isEmpty()) {
        return;
    }

    m_reconnecting = true;
    m_reconnectAttempt = 0;
    emit reconnectStateChanged(true, m_reconnectAttempt);
    m_reconnectTimer->start(m_reconnectInterval);
}

void SerialPortHelper::stopReconnect()
{
    m_reconnectTimer->stop();

    if (!m_reconnecting) {
        return;
    }

    m_reconnecting = false;
    emit reconnectStateChanged(false, m_reconnectAttempt);
}

void SerialPortHelper::reportError(QSerialPort::SerialPortError error, const QString &message)
{
    m_lastError = error;
    m_errorString = message;
    emit errorOccurred(error, message);
}
