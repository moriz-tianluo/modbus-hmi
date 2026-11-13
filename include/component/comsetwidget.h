#ifndef COMSETWIDGET_H
#define COMSETWIDGET_H

#include "base/BaseWidget.h"
#include "common/serialporthelper.h"

namespace Ui {
class ComSetWidget;
}

// 串口参数设置卡片：展示并让用户编辑 SerialPortConfig。
// MainWidget 会在打开串口前调用 getSerialPortConfig() 读取当前 UI 值。
class ComSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ComSetWidget(QWidget *parent = nullptr);
    ~ComSetWidget();

    void languageChange() override;

    // 从 UI 下拉框读取当前串口配置并组装为 SerialPortConfig 返回。
    // 调用方在 openPort 前获取此值；下拉框无有效选择时使用默认值兜底
    SerialPortConfig getSerialPortConfig() const;

    // 初始化端口下拉框为当前系统可用串口（调用 SerialPortHelper::availablePorts）
    void refreshPorts();

    // 响应超时（毫秒）与帧重试次数，MainWidget 在开始采集前下发给 SerialWorker
    int currentResponseTimeoutMs() const;
    int currentRetryCount() const;

    // 启动时用持久化配置回填界面（端口/波特率/帧格式/超时/重试）
    void applySettings(const SerialPortConfig &config, int responseTimeoutMs, int retryCount);

private:
    // 把 SerialPortConfig 的帧参数拼成下拉框文本，如 "8-N-1"
    QString frameTextOf(const SerialPortConfig &config) const;

private:
    Ui::ComSetWidget *ui;
};

#endif // COMSETWIDGET_H
