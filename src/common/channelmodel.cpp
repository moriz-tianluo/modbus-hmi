#include "channelmodel.h"
#include "Language.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>

#include <algorithm>

namespace {
const QString kKeyBufferSize = QStringLiteral("bufferSize");
const QString kKeyChannels = QStringLiteral("channels");
const QString kKeyName = QStringLiteral("name");
const QString kKeyUnit = QStringLiteral("unit");
const QString kKeySlave = QStringLiteral("slave");
const QString kKeyRegister = QStringLiteral("register");
const QString kKeyScale = QStringLiteral("scale");
const QString kKeyRangeMin = QStringLiteral("rangeMin");
const QString kKeyRangeMax = QStringLiteral("rangeMax");
const QString kKeyEnabled = QStringLiteral("enabled");
const QString kKeyDataType = QStringLiteral("dataType");
const QString kKeyDecimals = QStringLiteral("decimals");
const QString kKeyColor = QStringLiteral("color");
const QString kKeyAlarmEnabled = QStringLiteral("alarmEnabled");
const QString kKeyAlarmHigh = QStringLiteral("alarmHigh");
const QString kKeyAlarmLow = QStringLiteral("alarmLow");
}

double ChannelItem::value() const
{
    double raw = static_cast<double>(rawValue);
    // 有符号 16 位：最高位为 1 时按补码还原负值
    if (dataType == ChannelDataType::Signed16 && rawValue >= 0x8000) {
        raw -= 65536.0;
    }
    return raw * scale;
}

double ChannelItem::percent() const
{
    const double span = rangeMax - rangeMin;
    if (span <= 0.0) {
        return 0.0;
    }
    const double ratio = (value() - rangeMin) / span;
    return qBound(0.0, ratio * 100.0, 100.0);
}

ChannelModel::ChannelModel(QObject *parent)
    : QObject(parent)
{
}

ChannelModel::~ChannelModel() = default;

int ChannelModel::channelCount() const
{
    return m_channels.size();
}

const ChannelItem &ChannelModel::channelAt(int index) const
{
    return m_channels.at(index);
}

QVector<ChannelItem> ChannelModel::channels() const
{
    return m_channels;
}

void ChannelModel::setChannels(const QVector<ChannelItem> &channels)
{
    m_channels = channels;
    emit channelsChanged();
}

void ChannelModel::addChannel(const ChannelItem &item)
{
    m_channels.append(item);
    emit channelsChanged();
}

void ChannelModel::removeChannel(int index)
{
    if (index < 0 || index >= m_channels.size()) {
        return;
    }
    m_channels.removeAt(index);
    emit channelsChanged();
}

void ChannelModel::updateChannel(int index, const ChannelItem &item)
{
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    // 保留最近一次采集的原始值，其余配置整体替换
    ChannelItem updated = item;
    updated.rawValue = m_channels.at(index).rawValue;
    m_channels[index] = updated;
    emit channelsChanged();
}

void ChannelModel::clear()
{
    if (m_channels.isEmpty()) {
        return;
    }
    m_channels.clear();
    emit channelsChanged();
}

int ChannelModel::enabledCount() const
{
    int count = 0;
    for (const ChannelItem &item : m_channels) {
        if (item.enabled) {
            ++count;
        }
    }
    return count;
}

void ChannelModel::setChannelEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_channels.size()) {
        return;
    }
    if (m_channels.at(index).enabled == enabled) {
        return;
    }

    m_channels[index].enabled = enabled;
    emit channelEnabledChanged(index, enabled);
}

void ChannelModel::setAllChannelsEnabled(bool enabled)
{
    bool changed = false;
    for (ChannelItem &item : m_channels) {
        if (item.enabled != enabled) {
            item.enabled = enabled;
            changed = true;
        }
    }

    if (changed) {
        emit channelsChanged();
    }
}

int ChannelModel::indexOfAddress(quint16 address) const
{
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).registerAddress == address) {
            return index;
        }
    }
    return -1;
}

bool ChannelModel::validateAddressContinuity(QString *errorMessage) const
{
    return checkContinuity(m_channels, errorMessage);
}

bool ChannelModel::enabledAddressRange(quint16 &startAddress, quint16 &quantity) const
{
    bool found = false;
    quint16 minAddress = 0;
    quint16 maxAddress = 0;

    for (const ChannelItem &item : m_channels) {
        if (!item.enabled) {
            continue;
        }
        if (!found || item.registerAddress < minAddress) {
            minAddress = item.registerAddress;
        }
        if (!found || item.registerAddress > maxAddress) {
            maxAddress = item.registerAddress;
        }
        found = true;
    }

    if (!found) {
        return false;
    }

    startAddress = minAddress;
    quantity = static_cast<quint16>(maxAddress - minAddress + 1);
    return true;
}

bool ChannelModel::checkContinuity(const QVector<ChannelItem> &channels, QString *errorMessage) const
{
    // 多段轮询后不再要求地址连续，但仍不允许同一从站出现重复寄存器地址
    QSet<quint32> seenAddresses;
    for (const ChannelItem &item : channels) {
        if (!item.enabled) {
            continue;
        }
        const quint32 key = (static_cast<quint32>(item.slaveId) << 16) | item.registerAddress;
        if (seenAddresses.contains(key)) {
            setError(errorMessage, LANGUAGE("LG_channel_addrNotContiguous"));
            return false;
        }
        seenAddresses.insert(key);
    }
    return true;
}

QVector<PollTask> ChannelModel::buildPollTasks(ModbusFunctionCode functionCode) const
{
    const bool coilFunction = (functionCode == ModbusFunctionCode::ReadCoils
                               || functionCode == ModbusFunctionCode::ReadDiscreteInputs);
    const int maxQuantity = coilFunction ? 2000 : 125;

    // 先按从站归集启用通道地址
    QMap<quint8, QVector<quint16>> addressMap;
    for (const ChannelItem &item : m_channels) {
        if (item.enabled) {
            addressMap[item.slaveId].append(item.registerAddress);
        }
    }

    QVector<PollTask> tasks;
    for (auto it = addressMap.begin(); it != addressMap.end(); ++it) {
        QVector<quint16> addresses = it.value();
        std::sort(addresses.begin(), addresses.end());
        addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());

        // 同一从站内按地址连续性 + 单次读取上限切段
        int begin = 0;
        while (begin < addresses.size()) {
            int end = begin;
            while (end + 1 < addresses.size()
                   && addresses.at(end + 1) == addresses.at(end) + 1
                   && (addresses.at(end + 1) - addresses.at(begin) + 1) <= maxQuantity) {
                ++end;
            }

            PollTask task;
            task.slaveId = it.key();
            task.functionCode = functionCode;
            task.startAddress = addresses.at(begin);
            task.quantity = static_cast<quint16>(addresses.at(end) - addresses.at(begin) + 1);
            tasks.append(task);
            begin = end + 1;
        }
    }
    return tasks;
}

int ChannelModel::bufferSize() const
{
    return m_bufferSize;
}

void ChannelModel::setBufferSize(int count)
{
    const int clamped = qMax(1, count);
    if (m_bufferSize == clamped) {
        return;
    }
    m_bufferSize = clamped;
    emit bufferSizeChanged(m_bufferSize);
}

void ChannelModel::updateRawValues(quint16 startAddress, const QVector<quint16> &values)
{
    for (int offset = 0; offset < values.size(); ++offset) {
        const int index = indexOfAddress(static_cast<quint16>(startAddress + offset));
        if (index < 0 || !m_channels.at(index).enabled) {
            continue;
        }
        setChannelRawValue(index, values.at(offset));
    }
}

void ChannelModel::updateRawValues(quint8 slaveId, quint16 startAddress,
                                   const QVector<quint16> &values)
{
    for (int offset = 0; offset < values.size(); ++offset) {
        const quint16 address = static_cast<quint16>(startAddress + offset);
        for (int index = 0; index < m_channels.size(); ++index) {
            const ChannelItem &item = m_channels.at(index);
            if (!item.enabled || item.slaveId != slaveId || item.registerAddress != address) {
                continue;
            }
            setChannelRawValue(index, values.at(offset));
            break;
        }
    }
}

void ChannelModel::setChannelRawValue(int index, quint16 rawValue)
{
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    ChannelItem &item = m_channels[index];
    item.rawValue = rawValue;
    emit channelValueChanged(index, item.value());
}

bool ChannelModel::loadFromJson(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, LANGUAGE("LG_channel_jsonOpenFailed"));
        return false;
    }

    const QByteArray content = file.readAll();
    file.close();
    return fromJsonBytes(content, errorMessage);
}

bool ChannelModel::saveToJson(const QString &filePath, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage, LANGUAGE("LG_channel_jsonSaveFailed"));
        return false;
    }

    file.write(toJsonBytes());
    file.close();
    return true;
}

QByteArray ChannelModel::toJsonBytes() const
{
    QJsonArray channelArray;
    for (const ChannelItem &item : m_channels) {
        channelArray.append(channelToObject(item));
    }

    QJsonObject root;
    root.insert(kKeyBufferSize, m_bufferSize);
    root.insert(kKeyChannels, channelArray);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ChannelModel::fromJsonBytes(const QByteArray &json, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorMessage, LANGUAGE("LG_channel_jsonParseError"));
        return false;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(kKeyChannels) || !root.value(kKeyChannels).isArray()) {
        setError(errorMessage, LANGUAGE("LG_channel_jsonInvalidConfig"));
        return false;
    }

    QVector<ChannelItem> parsed;
    const QJsonArray channelArray = root.value(kKeyChannels).toArray();
    for (const QJsonValue &value : channelArray) {
        bool ok = false;
        const ChannelItem item = channelFromObject(value.toObject(), &ok);
        if (!ok) {
            setError(errorMessage, LANGUAGE("LG_channel_jsonInvalidConfig"));
            return false;
        }
        parsed.append(item);
    }

    // 导入时校验：启用通道的地址必须连续，否则轮询段无法覆盖
    if (!checkContinuity(parsed, errorMessage)) {
        return false;
    }

    setBufferSize(root.value(kKeyBufferSize).toInt(m_bufferSize));
    m_channels = parsed;
    emit channelsChanged();
    return true;
}

ChannelItem ChannelModel::channelFromObject(const QJsonObject &obj, bool *ok) const
{
    ChannelItem item;

    if (!obj.contains(kKeyName) || !obj.contains(kKeyRegister)) {
        if (ok) {
            *ok = false;
        }
        return item;
    }

    item.name = obj.value(kKeyName).toString();
    item.unit = obj.value(kKeyUnit).toString();
    item.slaveId = static_cast<quint8>(obj.value(kKeySlave).toInt(1));
    item.registerAddress = static_cast<quint16>(obj.value(kKeyRegister).toInt());
    item.dataType = static_cast<ChannelDataType>(obj.value(kKeyDataType).toInt(0));
    item.scale = obj.value(kKeyScale).toDouble(1.0);
    item.rangeMin = obj.value(kKeyRangeMin).toDouble(0.0);
    item.rangeMax = obj.value(kKeyRangeMax).toDouble(100.0);
    item.decimals = obj.value(kKeyDecimals).toInt(2);
    item.color = QColor(obj.value(kKeyColor).toString());
    item.enabled = obj.value(kKeyEnabled).toBool(true);
    item.alarmEnabled = obj.value(kKeyAlarmEnabled).toBool(false);
    item.alarmHigh = obj.value(kKeyAlarmHigh).toDouble(item.rangeMax);
    item.alarmLow = obj.value(kKeyAlarmLow).toDouble(item.rangeMin);
    item.rawValue = 0;

    if (ok) {
        *ok = true;
    }
    return item;
}

QJsonObject ChannelModel::channelToObject(const ChannelItem &item) const
{
    QJsonObject obj;
    obj.insert(kKeyName, item.name);
    obj.insert(kKeyUnit, item.unit);
    obj.insert(kKeySlave, item.slaveId);
    obj.insert(kKeyRegister, item.registerAddress);
    obj.insert(kKeyDataType, static_cast<int>(item.dataType));
    obj.insert(kKeyScale, item.scale);
    obj.insert(kKeyRangeMin, item.rangeMin);
    obj.insert(kKeyRangeMax, item.rangeMax);
    obj.insert(kKeyDecimals, item.decimals);
    obj.insert(kKeyColor, item.color.isValid() ? item.color.name() : QString());
    obj.insert(kKeyEnabled, item.enabled);
    obj.insert(kKeyAlarmEnabled, item.alarmEnabled);
    obj.insert(kKeyAlarmHigh, item.alarmHigh);
    obj.insert(kKeyAlarmLow, item.alarmLow);
    return obj;
}

void ChannelModel::setError(QString *errorMessage, const QString &text) const
{
    if (errorMessage) {
        *errorMessage = text;
    }
}
