#ifndef MODBUSSETWIDGET_H
#define MODBUSSETWIDGET_H

#include "base/BaseWidget.h"
#include "common/modbusrtu.h"

namespace Ui {
class ModbusSetWidget;
}

// Modbus 采集参数设置卡片（参数一站式面板）：
//   从站号 + 功能码 + 起始地址 + 读取数量 + 采集周期
// 所有 Modbus 协议层参数都在这里配置，MainWidget 在 startPolling 前从本卡读取。
class ModbusSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ModbusSetWidget(QWidget *parent = nullptr);
    ~ModbusSetWidget();

    void languageChange() override;

    // 从站号（comboBoxSlaveId 当前值），范围 1~247，非法时返回 1
    quint8 currentSlaveId() const;
    // 采集周期毫秒数（comboBoxInterval 当前值），默认 500ms
    int currentIntervalMs() const;

    // 从功能码下拉框读取当前选中项并转为 ModbusFunctionCode 枚举
    ModbusFunctionCode currentFunctionCode() const;
    // 功能码下拉框当前索引，供 ProjectConfig 持久化
    int currentFunctionCodeIndex() const;
    // 从起始地址下拉框读取（UI 用十六进制文本 "0x0000"）
    quint16 currentStartAddress() const;
    // 从数量下拉框读取（直接转 quint16）
    quint16 currentQuantity() const;

    // 启动时用持久化配置回填界面（功能码索引/起始地址/数量/从站号/采集周期）
    void applySettings(int functionCodeIndex, quint16 startAddress, quint16 quantity,
                       quint8 slaveId, int intervalMs);

signals:
    // 用户修改任一采集参数时发出，MainWidget 用于标记工程「未保存」
    void settingsChanged();

private:
    // 监听各参数下拉框，统一转成 settingsChanged 信号
    void initConnections();
    // 禁用当前版本不支持的 01/02 功能码选项
    void disableUnsupportedFunctionCodes();

private:
    Ui::ModbusSetWidget *ui;
};

#endif // MODBUSSETWIDGET_H
