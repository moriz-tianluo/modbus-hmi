#ifndef CHANNELMODEL_H
#define CHANNELMODEL_H

#include "polltask.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

// 寄存器数据类型：决定原始值按无符号还是有符号解释
enum class ChannelDataType {
    Unsigned16 = 0,  // 0~65535
    Signed16 = 1,    // -32768~32767（最高位为符号位）
};

// 单通道配置与实时数据
struct ChannelItem {
    QString name;                // 通道业务名称，如 "温度"、"压力"
    QString unit;                // 工程单位，如 "℃"、"MPa"
    quint8 slaveId = 1;          // 所属 Modbus 从站地址，支持同工程混合多从站
    quint16 registerAddress = 0; // 对应 Modbus 寄存器地址（起始 0）
    ChannelDataType dataType = ChannelDataType::Unsigned16;  // 寄存器数据类型
    double scale = 0.1;          // 换算系数：工程值 = 原始寄存器值 × scale
    double rangeMin = 0.0;       // 量程下限（工程值），用于 percent() 计算
    double rangeMax = 100.0;     // 量程上限（工程值），用于 percent() 计算
    int decimals = 2;            // 工程值显示小数位
    QColor color;                // 曲线/色点颜色，无效时由各控件按索引取默认色
    bool enabled = true;         // 是否参与采集（Poller 只读取启用通道覆盖的连续段）
    quint16 rawValue = 0;        // 最近一轮采集写入的原始寄存器值

    // ---- 报警规则 ----
    bool alarmEnabled = false;   // 是否启用越限报警
    double alarmHigh = 100.0;    // 报警上限（工程值）
    double alarmLow = 0.0;       // 报警下限（工程值）

    // 原始值换算为工程值。例：rawValue=812, scale=0.1 → 81.2
    // Signed16 且原始值最高位为 1 时按补码转换为负值
    double value() const;
    // 按量程归一化为百分比 [0, 100]，超出量程会被钳位。rangeMax-rangeMin <= 0 时返回 0
    double percent() const;
};

// 通道数据模型：管理通道列表、数值换算、JSON 配置导入导出、地址连续性校验。
// 纯数据模型，不含串口/定时器/线程依赖；运行时由 UI 线程持有，通过信号槽与 SerialWorker 交互。
// 约束：所有启用通道的寄存器地址必须连续且不重复，ModbusPoller 按连续段（startAddress + quantity）读取，
// 地址断档会导致段长度超出实际通道覆盖范围，多余寄存器被忽略。
class ChannelModel : public QObject
{
    Q_OBJECT

public:
    explicit ChannelModel(QObject *parent = nullptr);
    ~ChannelModel() override;

    // ---- 通道列表操作 ----
    int channelCount() const;                   // 总通道数（含禁用）
    const ChannelItem &channelAt(int index) const;  // 按索引取通道，越界会触发 QVector 断言
    QVector<ChannelItem> channels() const;      // 返回全部通道的拷贝
    void setChannels(const QVector<ChannelItem> &channels);  // 整体替换，发射 channelsChanged
    void addChannel(const ChannelItem &item);   // 追加通道，发射 channelsChanged
    void removeChannel(int index);              // 按索引移除（越界静默忽略），发射 channelsChanged
    // 替换指定通道的配置（保留该通道最近的 rawValue），越界静默忽略，发射 channelsChanged
    void updateChannel(int index, const ChannelItem &item);
    void clear();                               // 清空全部通道，发射 channelsChanged
    int enabledCount() const;                   // 启用通道数量

    // 切换单个通道的启用状态：只更新数据并发射 channelEnabledChanged，
    // 不发射 channelsChanged，调用方（通道表）自行刷新该行样式，避免重建表格
    void setChannelEnabled(int index, bool enabled);
    // 批量切换全部通道的启用状态，发射 channelsChanged 以触发视图整体重建
    void setAllChannelsEnabled(bool enabled);

    // 按寄存器地址查找通道下标，未找到返回 -1。线性扫描，通道数量大时慎用
    int indexOfAddress(quint16 address) const;

    // 把启用通道按（从站 + 功能码 + 地址连续）切分为轮询任务。
    // 同从站地址断档会拆成多段，超过单次读取上限会继续切分；返回结果可直接交给 SerialWorker。
    QVector<PollTask> buildPollTasks(ModbusFunctionCode functionCode) const;

    // 校验启用通道的寄存器地址是否连续且不重复。
    // 内部逻辑：收集所有 enabled=true 的地址 → 升序排序 → 逐项检查是否为前一项 + 1。
    // 少于 2 个启用通道时自动视为通过。
    // 失败时若 errorMessage 非空，写入国际化错误文案（"地址不连续或重复"）。
    bool validateAddressContinuity(QString *errorMessage = nullptr) const;

    // 计算启用通道覆盖的连续地址段范围。
    // startAddress = min(启用通道地址), quantity = max(启用通道地址) - min + 1。
    // 无启用通道时返回 false；调用方应在 startPolling 前先调 validateAddressContinuity 确保 quantity 有效。
    bool enabledAddressRange(quint16 &startAddress, quint16 &quantity) const;

    // ---- 曲线缓存点数 ----
    int bufferSize() const;             // UI 侧显示历史曲线的最大缓存点数（通道数量 × 周期数）
    void setBufferSize(int count);       // 设置缓存点数，<1 会被钳为 1，发射 bufferSizeChanged

    // ---- 数据更新 ----
    // 用一轮 Modbus 采集到的寄存器值批量更新通道（不区分从站，兼容旧调用）。
    // 映射逻辑：values[offset] → 寄存器地址 (startAddress + offset) 对应的 ChannelItem。
    // 地址未注册或通道已禁用则跳过，匹配到的通道写入 rawValue 并发射 channelValueChanged。
    void updateRawValues(quint16 startAddress, const QVector<quint16> &values);
    // 多从站版本：额外按 slaveId 过滤，避免同地址不同从站串数据
    void updateRawValues(quint8 slaveId, quint16 startAddress, const QVector<quint16> &values);
    // 单通道原始值更新，index 越界静默忽略，成功后发射 channelValueChanged
    void setChannelRawValue(int index, quint16 rawValue);

    // ---- JSON 配置导入导出 ----
    // 从文件导入通道配置并替换当前列表。失败时写 errorMessage 并返回 false。
    // 导入后自动校验地址连续性，不通过则整体拒绝（不覆盖已有配置）。
    bool loadFromJson(const QString &filePath, QString *errorMessage = nullptr);
    // 将当前通道列表保存到文件，覆盖写入。失败时写 errorMessage 并返回 false。
    bool saveToJson(const QString &filePath, QString *errorMessage = nullptr) const;
    // 序列化为 JSON 字节流（bufferSize + channels 数组，带缩进）
    QByteArray toJsonBytes() const;
    // 从 JSON 字节流解析通道配置，替换当前列表。
    // 失败原因：JSON 语法错误、缺 channels 字段、通道对象缺 name/register、地址不连续。
    bool fromJsonBytes(const QByteArray &json, QString *errorMessage = nullptr);

signals:
    void channelsChanged();                    // 通道列表整体变化（增/删/清空/导入/整体替换）
    void channelValueChanged(int index, double value);  // 某通道工程值更新：index 为通道索引，value = rawValue × scale
    void channelEnabledChanged(int index, bool enabled);  // 单通道启用状态切换
    void bufferSizeChanged(int count);         // 缓存点数变化

private:
    ChannelItem channelFromObject(const QJsonObject &obj, bool *ok) const;
    QJsonObject channelToObject(const ChannelItem &item) const;
    void setError(QString *errorMessage, const QString &text) const;
    bool checkContinuity(const QVector<ChannelItem> &channels, QString *errorMessage) const;

    QVector<ChannelItem> m_channels;
    int m_bufferSize = 2000;
};

#endif // CHANNELMODEL_H
