#include "channelmodel.h"
#include "Language.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {
const QString kKeyBufferSize = QStringLiteral("bufferSize");
const QString kKeyChannels = QStringLiteral("channels");
const QString kKeyName = QStringLiteral("name");
const QString kKeyUnit = QStringLiteral("unit");
const QString kKeyRegister = QStringLiteral("register");
const QString kKeyScale = QStringLiteral("scale");
const QString kKeyRangeMin = QStringLiteral("rangeMin");
const QString kKeyRangeMax = QStringLiteral("rangeMax");
const QString kKeyEnabled = QStringLiteral("enabled");
}

double ChannelItem::value() const
{
    return rawValue * scale;
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
    QVector<quint16> addresses;
    for (const ChannelItem &item : channels) {
        if (item.enabled) {
            addresses.append(item.registerAddress);
        }
    }
    if (addresses.size() < 2) {
        return true;
    }

    std::sort(addresses.begin(), addresses.end());
    for (int index = 1; index < addresses.size(); ++index) {
        // 重复或跳号都视为不连续
        if (addresses.at(index) != addresses.at(index - 1) + 1) {
            setError(errorMessage, LANGUAGE("LG_channel_addrNotContiguous"));
            return false;
        }
    }
    return true;
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
    item.registerAddress = static_cast<quint16>(obj.value(kKeyRegister).toInt());
    item.scale = obj.value(kKeyScale).toDouble(1.0);
    item.rangeMin = obj.value(kKeyRangeMin).toDouble(0.0);
    item.rangeMax = obj.value(kKeyRangeMax).toDouble(100.0);
    item.enabled = obj.value(kKeyEnabled).toBool(true);
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
    obj.insert(kKeyRegister, item.registerAddress);
    obj.insert(kKeyScale, item.scale);
    obj.insert(kKeyRangeMin, item.rangeMin);
    obj.insert(kKeyRangeMax, item.rangeMax);
    obj.insert(kKeyEnabled, item.enabled);
    return obj;
}

void ChannelModel::setError(QString *errorMessage, const QString &text) const
{
    if (errorMessage) {
        *errorMessage = text;
    }
}
