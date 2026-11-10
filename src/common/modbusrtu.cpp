#include "modbusrtu.h"
#include "serialporthelper.h"
#include "Language.h"

#include <QTimer>

ModbusRtu::ModbusRtu(SerialPortHelper *serial, QObject *parent)
    : QObject(parent)
    , m_serial(serial)
    , m_timeoutTimer(new QTimer(this))
{
    qRegisterMetaType<ModbusError>("ModbusError");

    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ModbusRtu::onResponseTimeout);

    if (m_serial) {
        connect(m_serial, &SerialPortHelper::frameReceived, this, &ModbusRtu::onFrameReceived);
        connect(m_serial, &SerialPortHelper::portClosed, this, &ModbusRtu::onPortClosed);
    }
}

ModbusRtu::~ModbusRtu()
{
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
}

void ModbusRtu::setSlaveId(quint8 slaveId)
{
    m_slaveId = slaveId;
}

quint8 ModbusRtu::slaveId() const
{
    return m_slaveId;
}

void ModbusRtu::setResponseTimeout(int milliseconds)
{
    m_responseTimeout = qMax(50, milliseconds);
}

int ModbusRtu::responseTimeout() const
{
    return m_responseTimeout;
}

void ModbusRtu::setMaxRetries(int count)
{
    m_maxRetries = qMax(0, count);
}

int ModbusRtu::maxRetries() const
{
    return m_maxRetries;
}

bool ModbusRtu::isBusy() const
{
    return m_busy;
}

bool ModbusRtu::readRegisters(ModbusFunctionCode code, quint16 startAddress, quint16 quantity)
{
    if (!m_serial || !m_serial->isOpen()) {
        emit requestFailed(ModbusError::SerialError, LANGUAGE("LG_modbus_serialNotOpen"));
        return false;
    }
    if (m_busy) {
        emit requestFailed(ModbusError::Busy, LANGUAGE("LG_modbus_busy"));
        return false;
    }
    if (!validateQuantity(code, quantity)) {
        emit requestFailed(ModbusError::IllegalQuantity, LANGUAGE("LG_modbus_illegalQuantity"));
        return false;
    }

    m_functionCode = code;
    m_startAddress = startAddress;
    m_quantity = quantity;
    m_retryCount = 0;
    m_request = buildRequest(m_slaveId, static_cast<quint8>(code), startAddress, quantity);

    startTransaction(m_request);
    return true;
}

quint16 ModbusRtu::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int index = 0; index < data.size(); ++index) {
        crc ^= static_cast<quint8>(data.at(index));
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

QByteArray ModbusRtu::buildRequest(quint8 slaveId, quint8 functionCode,
                                   quint16 startAddress, quint16 quantity)
{
    QByteArray frame;
    frame.append(static_cast<char>(slaveId));
    frame.append(static_cast<char>(functionCode));
    frame.append(static_cast<char>((startAddress >> 8) & 0xFF));
    frame.append(static_cast<char>(startAddress & 0xFF));
    frame.append(static_cast<char>((quantity >> 8) & 0xFF));
    frame.append(static_cast<char>(quantity & 0xFF));

    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

bool ModbusRtu::parseResponse(const QByteArray &frame, ModbusFrame *result, ModbusError *error)
{
    if (!result || !error) {
        return false;
    }

    *result = ModbusFrame();
    *error = ModbusError::NoError;

    // 最短应答：从站 + 功能码 + 字节数 + 1 字节数据 + 2 字节 CRC
    if (frame.size() < 5) {
        *error = ModbusError::InvalidFrame;
        return false;
    }

    const QByteArray data = frame.left(frame.size() - 2);
    const quint16 receivedCrc = static_cast<quint8>(frame.at(frame.size() - 2))
                                | (static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 1))) << 8);
    if (crc16(data) != receivedCrc) {
        *error = ModbusError::CrcError;
        return false;
    }

    result->slaveId = static_cast<quint8>(data.at(0));
    result->functionCode = static_cast<quint8>(data.at(1));

    // 异常应答：功能码最高位置 1，后续跟异常码
    if (result->functionCode & 0x80) {
        if (data.size() < 3) {
            *error = ModbusError::InvalidFrame;
            return false;
        }
        result->exceptionCode = static_cast<quint8>(data.at(2));
        *error = ModbusError::ExceptionCode;
        return false;
    }

    const int byteCount = static_cast<quint8>(data.at(2));
    if (data.size() != byteCount + 3) {
        *error = ModbusError::ByteCountMismatch;
        return false;
    }
    result->payload = data.mid(3, byteCount);

    if (result->functionCode == static_cast<quint8>(ModbusFunctionCode::ReadHoldingRegisters)
        || result->functionCode == static_cast<quint8>(ModbusFunctionCode::ReadInputRegisters)) {
        if (byteCount % 2 != 0) {
            *error = ModbusError::ByteCountMismatch;
            return false;
        }
        result->registers.reserve(byteCount / 2);
        for (int index = 0; index < byteCount; index += 2) {
            const quint16 value = (static_cast<quint16>(static_cast<quint8>(result->payload.at(index))) << 8)
                                  | static_cast<quint8>(result->payload.at(index + 1));
            result->registers.append(value);
        }
        return true;
    }

    if (result->functionCode == static_cast<quint8>(ModbusFunctionCode::ReadCoils)
        || result->functionCode == static_cast<quint8>(ModbusFunctionCode::ReadDiscreteInputs)) {
        result->bits.reserve(byteCount * 8);
        for (int index = 0; index < byteCount; ++index) {
            const quint8 value = static_cast<quint8>(result->payload.at(index));
            for (int bit = 0; bit < 8; ++bit) {
                result->bits.append((value >> bit) & 0x01);
            }
        }
        return true;
    }

    *error = ModbusError::InvalidFrame;
    return false;
}

QString ModbusRtu::errorString(ModbusError error)
{
    switch (error) {
    case ModbusError::NoError:
        return LANGUAGE("LG_modbus_noError");
    case ModbusError::InvalidFrame:
        return LANGUAGE("LG_modbus_invalidFrame");
    case ModbusError::CrcError:
        return LANGUAGE("LG_modbus_crcError");
    case ModbusError::SlaveMismatch:
        return LANGUAGE("LG_modbus_slaveMismatch");
    case ModbusError::FunctionMismatch:
        return LANGUAGE("LG_modbus_functionMismatch");
    case ModbusError::ByteCountMismatch:
        return LANGUAGE("LG_modbus_byteCountMismatch");
    case ModbusError::IllegalQuantity:
        return LANGUAGE("LG_modbus_illegalQuantity");
    case ModbusError::ExceptionCode:
        return LANGUAGE("LG_modbus_exception");
    case ModbusError::Timeout:
        return LANGUAGE("LG_modbus_timeout");
    case ModbusError::SerialError:
        return LANGUAGE("LG_modbus_serialError");
    case ModbusError::Busy:
        return LANGUAGE("LG_modbus_busy");
    }
    return LANGUAGE("LG_modbus_unknownError");
}

QString ModbusRtu::exceptionString(quint8 exceptionCode)
{
    switch (exceptionCode) {
    case 0x01:
        return LANGUAGE("LG_modbus_exc_illegalFunction");
    case 0x02:
        return LANGUAGE("LG_modbus_exc_illegalAddress");
    case 0x03:
        return LANGUAGE("LG_modbus_exc_illegalValue");
    case 0x04:
        return LANGUAGE("LG_modbus_exc_slaveFault");
    case 0x05:
        return LANGUAGE("LG_modbus_exc_confirm");
    case 0x06:
        return LANGUAGE("LG_modbus_exc_slaveBusy");
    case 0x08:
        return LANGUAGE("LG_modbus_exc_parityError");
    case 0x0A:
        return LANGUAGE("LG_modbus_exc_gatewayPath");
    case 0x0B:
        return LANGUAGE("LG_modbus_exc_gatewayTarget");
    default:
        return LANGUAGE("LG_modbus_exc_unknown");
    }
}

void ModbusRtu::onFrameReceived(const QByteArray &frame)
{
    if (!m_busy) {
        return;
    }

    const qint64 rtt = m_rttTimer.elapsed();
    m_timeoutTimer->stop();
    handleFrame(frame, rtt);
}

void ModbusRtu::onResponseTimeout()
{
    if (!m_busy) {
        return;
    }
    retryOrFail(ModbusError::Timeout, errorString(ModbusError::Timeout));
}

void ModbusRtu::onPortClosed(const QString &portName)
{
    Q_UNUSED(portName)

    if (!m_busy) {
        return;
    }

    finishTransaction();
    emit requestFailed(ModbusError::SerialError, LANGUAGE("LG_modbus_serialInterrupted"));
}

bool ModbusRtu::validateQuantity(ModbusFunctionCode code, quint16 quantity) const
{
    if (quantity == 0) {
        return false;
    }

    const bool isBits = (code == ModbusFunctionCode::ReadCoils
                         || code == ModbusFunctionCode::ReadDiscreteInputs);
    return isBits ? quantity <= kMaxCoilsPerRead : quantity <= kMaxRegistersPerRead;
}

void ModbusRtu::startTransaction(const QByteArray &request)
{
    m_busy = true;
    m_rttTimer.restart();
    m_timeoutTimer->start(m_responseTimeout);

    if (m_serial->send(request)) {
        return;
    }

    finishTransaction();
    emit requestFailed(ModbusError::SerialError, LANGUAGE("LG_modbus_sendFailed"));
}

void ModbusRtu::handleFrame(const QByteArray &frame, qint64 rttMs)
{
    ModbusFrame parsed;
    ModbusError error = ModbusError::NoError;

    if (!parseResponse(frame, &parsed, &error)) {
        if (error == ModbusError::CrcError || error == ModbusError::InvalidFrame) {
            // 干扰帧按失败重试处理
            retryOrFail(error, errorString(error));
            return;
        }

        finishTransaction();
        if (error == ModbusError::ExceptionCode) {
            emit exceptionReceived(parsed.exceptionCode, exceptionString(parsed.exceptionCode));
        } else {
            emit requestFailed(error, errorString(error));
        }
        return;
    }

    // 非本次请求的应答（地址或功能码不符）直接丢弃，等待正确应答或超时
    if (parsed.slaveId != m_slaveId
        || parsed.functionCode != static_cast<quint8>(m_functionCode)) {
        m_timeoutTimer->start(m_responseTimeout);
        return;
    }

    finishTransaction();

    if (m_functionCode == ModbusFunctionCode::ReadHoldingRegisters
        || m_functionCode == ModbusFunctionCode::ReadInputRegisters) {
        emit registersRead(m_startAddress, parsed.registers, rttMs);
        return;
    }

    QVector<bool> values = parsed.bits;
    values.resize(m_quantity);
    emit bitsRead(m_startAddress, values, rttMs);
}

void ModbusRtu::retryOrFail(ModbusError error, const QString &message)
{
    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        m_rttTimer.restart();
        m_timeoutTimer->start(m_responseTimeout);
        m_serial->send(m_request);
        return;
    }

    finishTransaction();
    emit requestFailed(error, message);
}

void ModbusRtu::finishTransaction()
{
    m_timeoutTimer->stop();
    m_busy = false;
    m_retryCount = 0;
    m_request.clear();
}
