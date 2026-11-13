#ifndef PROJECTCONFIG_H
#define PROJECTCONFIG_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QSettings;

// 项目级配置持久化类。
// 落地文件：<程序目录>/data/project.ini（QSettings::IniFormat），
// 程序目录不可写时退化到系统配置目录，避免安装到 Program Files 后写失败。
// 职责：
//   - 串口参数（端口/波特率/帧格式/响应超时/重试次数）记忆，下次启动自动回填
//   - Modbus 采集参数（功能码/起始地址/读取数量/从站号/采集周期）记忆
//   - 主窗口几何、通道配置 JSON 路径、落库开关、曲线缓冲点数
//   - 不涉及通道本身的业务数据（那是 ChannelModel 的职责）
class ProjectConfig : public QObject
{
    Q_OBJECT

public:
    // 串口参数（含响应超时与重试次数，语义比 SerialPortConfig 更宽）
    struct SerialSettings {
        QString portName;
        qint32 baudRate = 9600;
        int dataBits = 8;              // QSerialPort::DataBits 的整型值
        int parity = 0;                // QSerialPort::Parity 的整型值
        int stopBits = 1;              // QSerialPort::StopBits 的整型值
        int responseTimeoutMs = 500;   // 单帧响应超时
        int retryCount = 3;            // 单帧重试次数
    };

    // Modbus 采集参数
    struct ModbusSettings {
        int functionCodeIndex = 0;     // 与 ModbusSetWidget 功能码下拉框索引一致
        quint16 startAddress = 0;      // 起始寄存器地址
        quint16 quantity = 4;          // 单次读取数量
        quint8 slaveId = 1;            // 从站号
        int intervalMs = 500;          // 采集周期
    };

    explicit ProjectConfig(QObject *parent = nullptr);

    // 从 ini 读取全部配置；文件不存在时保留各字段默认值
    void load();
    // 把内存中的配置写回 ini 并同步到磁盘
    void save();

    SerialSettings serialSettings() const;
    void setSerialSettings(const SerialSettings &settings);

    ModbusSettings modbusSettings() const;
    void setModbusSettings(const ModbusSettings &settings);

    // 主窗口几何（restoreGeometry/saveGeometry 的字节流）
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    // 最近一次导入/导出的通道配置 JSON 路径
    QString channelConfigPath() const;
    void setChannelConfigPath(const QString &path);

    // 采集数据是否同时写入 SQLite
    bool saveToDatabase() const;
    void setSaveToDatabase(bool enabled);

    // 曲线缓冲点数
    int bufferSize() const;
    void setBufferSize(int size);

    // 实时曲线保留时长（秒）
    int curveWindowSeconds() const;
    void setCurveWindowSeconds(int seconds);

    // 报警去抖：连续越限多少次才记录一条报警（默认 3）
    int alarmDebounceCount() const;
    void setAlarmDebounceCount(int count);

    // ---- 工程文件（*.mhproj）----
    // 当前工程文件路径，新建工程后为空
    QString currentProjectPath() const;
    void setCurrentProjectPath(const QString &path);
    // 最近打开的工程文件列表（最多 5 条，最新在前）
    QStringList recentProjects() const;
    // 追加一条最近工程并持久化；为空或重复时只做去重前置
    void addRecentProject(const QString &path);

    // ---- 数据保留 ----
    // 采样数据保留天数，<=0 表示不自动清理
    int dataRetentionDays() const;
    void setDataRetentionDays(int days);

    // ---- 曲线外观 ----
    int curveLineWidth() const;
    void setCurveLineWidth(int width);
    // 上限参考线数值（纵轴 0~100 归一化）
    double curveUpperLimit() const;
    void setCurveUpperLimit(double value);
    bool curveUpperLimitVisible() const;
    void setCurveUpperLimitVisible(bool visible);

private:
    // ini 文件完整路径（含目录创建，目录不可写时退化到系统配置目录）
    QString settingsFilePath() const;

private:
    QSettings *m_settings = nullptr;

    // ---- 持久化数据 ----
    SerialSettings m_serialSettings;
    ModbusSettings m_modbusSettings;
    QByteArray m_windowGeometry;
    QString m_channelConfigPath;
    bool m_saveToDatabase = true;
    int m_bufferSize = 60;
    int m_curveWindowSeconds = 300;  // 实时曲线默认保留 5 分钟
    int m_alarmDebounceCount = 3;    // 连续越限次数阈值

    QString m_currentProjectPath;
    QStringList m_recentProjects;
    int m_dataRetentionDays = 0;     // 0 表示不自动清理
    int m_curveLineWidth = 2;        // 曲线线宽（像素）
    double m_curveUpperLimit = 80.0; // 上限参考线坐标
    bool m_curveUpperLimitVisible = true;
};

#endif // PROJECTCONFIG_H
