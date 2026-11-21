#include "projectconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

ProjectConfig::ProjectConfig(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(settingsFilePath(), QSettings::IniFormat, this))
{
}

QString ProjectConfig::settingsFilePath() const
{
    // 优先放在程序目录 data/ 下，便于打包后随程序一起携带
    const QString dataDir = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("data"));
    if (QDir().mkpath(dataDir)) {
        return QDir(dataDir).filePath(QStringLiteral("project.ini"));
    }

    // 程序目录不可写（如安装在 Program Files）时退化到系统配置目录
    const QString fallbackDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(fallbackDir);
    return QDir(fallbackDir).filePath(QStringLiteral("project.ini"));
}

void ProjectConfig::load()
{
    // ---- 串口参数 ----
    m_settings->beginGroup(QStringLiteral("serial"));
    m_serialSettings.portName =
        m_settings->value(QStringLiteral("portName"), m_serialSettings.portName).toString();
    m_serialSettings.baudRate =
        m_settings->value(QStringLiteral("baudRate"), m_serialSettings.baudRate).toInt();
    m_serialSettings.dataBits =
        m_settings->value(QStringLiteral("dataBits"), m_serialSettings.dataBits).toInt();
    m_serialSettings.parity =
        m_settings->value(QStringLiteral("parity"), m_serialSettings.parity).toInt();
    m_serialSettings.stopBits =
        m_settings->value(QStringLiteral("stopBits"), m_serialSettings.stopBits).toInt();
    m_serialSettings.responseTimeoutMs =
        m_settings->value(QStringLiteral("responseTimeoutMs"),
                          m_serialSettings.responseTimeoutMs).toInt();
    m_serialSettings.retryCount =
        m_settings->value(QStringLiteral("retryCount"), m_serialSettings.retryCount).toInt();
    m_settings->endGroup();

    // ---- Modbus 采集参数 ----
    m_settings->beginGroup(QStringLiteral("modbus"));
    m_modbusSettings.functionCodeIndex =
        m_settings->value(QStringLiteral("functionCodeIndex"),
                          m_modbusSettings.functionCodeIndex).toInt();
    m_modbusSettings.startAddress =
        m_settings->value(QStringLiteral("startAddress"),
                          m_modbusSettings.startAddress).toUInt();
    m_modbusSettings.quantity =
        m_settings->value(QStringLiteral("quantity"),
                          m_modbusSettings.quantity).toUInt();
    m_modbusSettings.slaveId =
        m_settings->value(QStringLiteral("slaveId"),
                          m_modbusSettings.slaveId).toUInt();
    m_modbusSettings.intervalMs =
        m_settings->value(QStringLiteral("intervalMs"),
                          m_modbusSettings.intervalMs).toInt();
    m_settings->endGroup();

    // ---- 界面与运行偏好 ----
    m_settings->beginGroup(QStringLiteral("ui"));
    m_windowGeometry = m_settings->value(QStringLiteral("windowGeometry")).toByteArray();
    m_channelConfigPath =
        m_settings->value(QStringLiteral("channelConfigPath"), m_channelConfigPath).toString();
    m_saveToDatabase =
        m_settings->value(QStringLiteral("saveToDatabase"), m_saveToDatabase).toBool();
    m_bufferSize = m_settings->value(QStringLiteral("bufferSize"), m_bufferSize).toInt();
    m_curveWindowSeconds =
        m_settings->value(QStringLiteral("curveWindowSeconds"), m_curveWindowSeconds).toInt();
    m_alarmDebounceCount =
        m_settings->value(QStringLiteral("alarmDebounceCount"), m_alarmDebounceCount).toInt();
    m_languageType =
        m_settings->value(QStringLiteral("languageType"), m_languageType).toInt();
    m_settings->endGroup();

    // ---- 工程文件与数据保留 ----
    m_settings->beginGroup(QStringLiteral("project"));
    m_currentProjectPath =
        m_settings->value(QStringLiteral("currentPath"), m_currentProjectPath).toString();
    m_recentProjects =
        m_settings->value(QStringLiteral("recentProjects"), m_recentProjects).toStringList();
    m_dataRetentionDays =
        m_settings->value(QStringLiteral("dataRetentionDays"), m_dataRetentionDays).toInt();
    m_curveLineWidth =
        m_settings->value(QStringLiteral("curveLineWidth"), m_curveLineWidth).toInt();
    m_curveUpperLimit =
        m_settings->value(QStringLiteral("curveUpperLimit"), m_curveUpperLimit).toDouble();
    m_curveUpperLimitVisible =
        m_settings->value(QStringLiteral("curveUpperLimitVisible"),
                          m_curveUpperLimitVisible).toBool();
    m_settings->endGroup();

    // 非法缓冲点数回落默认值，避免曲线采样上限为 0
    if (m_bufferSize <= 0) {
        m_bufferSize = 60;
    }
    // 曲线窗口非法时回落默认 5 分钟
    if (m_curveWindowSeconds <= 0) {
        m_curveWindowSeconds = 300;
    }
    // 去抖次数非法时回落默认 3 次
    m_alarmDebounceCount = qBound(1, m_alarmDebounceCount, 100);
    // 线宽与上限线取值收敛到界面允许范围
    m_curveLineWidth = qBound(1, m_curveLineWidth, 6);
    m_curveUpperLimit = qBound(0.0, m_curveUpperLimit, 100.0);
    m_dataRetentionDays = qMax(0, m_dataRetentionDays);
    // 语言取值收敛到 -1/0/1，防止手改 ini 造成非法值
    m_languageType = qBound(-1, m_languageType, 1);
}

void ProjectConfig::save()
{
    m_settings->beginGroup(QStringLiteral("serial"));
    m_settings->setValue(QStringLiteral("portName"), m_serialSettings.portName);
    m_settings->setValue(QStringLiteral("baudRate"), m_serialSettings.baudRate);
    m_settings->setValue(QStringLiteral("dataBits"), m_serialSettings.dataBits);
    m_settings->setValue(QStringLiteral("parity"), m_serialSettings.parity);
    m_settings->setValue(QStringLiteral("stopBits"), m_serialSettings.stopBits);
    m_settings->setValue(QStringLiteral("responseTimeoutMs"),
                         m_serialSettings.responseTimeoutMs);
    m_settings->setValue(QStringLiteral("retryCount"), m_serialSettings.retryCount);
    m_settings->endGroup();

    m_settings->beginGroup(QStringLiteral("modbus"));
    m_settings->setValue(QStringLiteral("functionCodeIndex"), m_modbusSettings.functionCodeIndex);
    m_settings->setValue(QStringLiteral("startAddress"), m_modbusSettings.startAddress);
    m_settings->setValue(QStringLiteral("quantity"), m_modbusSettings.quantity);
    m_settings->setValue(QStringLiteral("slaveId"), m_modbusSettings.slaveId);
    m_settings->setValue(QStringLiteral("intervalMs"), m_modbusSettings.intervalMs);
    m_settings->endGroup();

    m_settings->beginGroup(QStringLiteral("ui"));
    m_settings->setValue(QStringLiteral("windowGeometry"), m_windowGeometry);
    m_settings->setValue(QStringLiteral("channelConfigPath"), m_channelConfigPath);
    m_settings->setValue(QStringLiteral("saveToDatabase"), m_saveToDatabase);
    m_settings->setValue(QStringLiteral("bufferSize"), m_bufferSize);
    m_settings->setValue(QStringLiteral("curveWindowSeconds"), m_curveWindowSeconds);
    m_settings->setValue(QStringLiteral("alarmDebounceCount"), m_alarmDebounceCount);
    m_settings->setValue(QStringLiteral("languageType"), m_languageType);
    m_settings->endGroup();

    m_settings->beginGroup(QStringLiteral("project"));
    m_settings->setValue(QStringLiteral("currentPath"), m_currentProjectPath);
    m_settings->setValue(QStringLiteral("recentProjects"), m_recentProjects);
    m_settings->setValue(QStringLiteral("dataRetentionDays"), m_dataRetentionDays);
    m_settings->setValue(QStringLiteral("curveLineWidth"), m_curveLineWidth);
    m_settings->setValue(QStringLiteral("curveUpperLimit"), m_curveUpperLimit);
    m_settings->setValue(QStringLiteral("curveUpperLimitVisible"), m_curveUpperLimitVisible);
    m_settings->endGroup();

    // 立即落盘，避免程序异常退出时丢配置
    m_settings->sync();
}

ProjectConfig::SerialSettings ProjectConfig::serialSettings() const
{
    return m_serialSettings;
}

void ProjectConfig::setSerialSettings(const SerialSettings &settings)
{
    m_serialSettings = settings;
}

ProjectConfig::ModbusSettings ProjectConfig::modbusSettings() const
{
    return m_modbusSettings;
}

void ProjectConfig::setModbusSettings(const ModbusSettings &settings)
{
    m_modbusSettings = settings;
}

QByteArray ProjectConfig::windowGeometry() const
{
    return m_windowGeometry;
}

void ProjectConfig::setWindowGeometry(const QByteArray &geometry)
{
    m_windowGeometry = geometry;
}

QString ProjectConfig::channelConfigPath() const
{
    return m_channelConfigPath;
}

void ProjectConfig::setChannelConfigPath(const QString &path)
{
    m_channelConfigPath = path;
}

bool ProjectConfig::saveToDatabase() const
{
    return m_saveToDatabase;
}

void ProjectConfig::setSaveToDatabase(bool enabled)
{
    m_saveToDatabase = enabled;
}

int ProjectConfig::bufferSize() const
{
    return m_bufferSize;
}

void ProjectConfig::setBufferSize(int size)
{
    if (size > 0) {
        m_bufferSize = size;
    }
}

int ProjectConfig::curveWindowSeconds() const
{
    return m_curveWindowSeconds;
}

void ProjectConfig::setCurveWindowSeconds(int seconds)
{
    if (seconds > 0) {
        m_curveWindowSeconds = seconds;
    }
}

int ProjectConfig::alarmDebounceCount() const
{
    return m_alarmDebounceCount;
}

void ProjectConfig::setAlarmDebounceCount(int count)
{
    m_alarmDebounceCount = qBound(1, count, 100);
}

QString ProjectConfig::currentProjectPath() const
{
    return m_currentProjectPath;
}

void ProjectConfig::setCurrentProjectPath(const QString &path)
{
    m_currentProjectPath = path;
}

QStringList ProjectConfig::recentProjects() const
{
    return m_recentProjects;
}

void ProjectConfig::addRecentProject(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    // 已存在则先移除，保证最新打开的排在最前
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    while (m_recentProjects.size() > 5) {
        m_recentProjects.removeLast();
    }
    save();
}

int ProjectConfig::dataRetentionDays() const
{
    return m_dataRetentionDays;
}

void ProjectConfig::setDataRetentionDays(int days)
{
    m_dataRetentionDays = qMax(0, days);
}

int ProjectConfig::curveLineWidth() const
{
    return m_curveLineWidth;
}

void ProjectConfig::setCurveLineWidth(int width)
{
    m_curveLineWidth = qBound(1, width, 6);
}

double ProjectConfig::curveUpperLimit() const
{
    return m_curveUpperLimit;
}

void ProjectConfig::setCurveUpperLimit(double value)
{
    m_curveUpperLimit = qBound(0.0, value, 100.0);
}

bool ProjectConfig::curveUpperLimitVisible() const
{
    return m_curveUpperLimitVisible;
}

void ProjectConfig::setCurveUpperLimitVisible(bool visible)
{
    m_curveUpperLimitVisible = visible;
}

int ProjectConfig::languageType() const
{
    return m_languageType;
}

void ProjectConfig::setLanguageType(int type)
{
    m_languageType = qBound(-1, type, 1);
}
