#ifndef OPERATIONWIDGET_H
#define OPERATIONWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class operationWidget;
}

// 顶部操作工具条（纯操作栏）：只放启停按钮 + 状态显示。
// 串口参数和 Modbus 参数全部在左侧卡片（comSetWidget / modbusSetWidget）配置，
// 本类只负责把按钮点击事件转成信号让 MainWidget 汇总处理。
class operationWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit operationWidget(QWidget *parent = nullptr);
    ~operationWidget();

    void languageChange() override;

    // ---- 状态刷新（由 MainWidget 在 SerialWorker 状态变化时调用）----
    void updateConnectionState(bool opened);   // 串口打开/关闭 → 切换按钮文字 + 状态标签
    void updatePollingState(bool running);      // 采集运行/停止 → 切换启停按钮文字
    void updateCurvePauseState(bool paused);    // 曲线冻结/恢复 → 切换暂停曲线按钮文字
    void setStatusText(const QString &text);   // 直接覆盖状态标签文字（错误提示等）

signals:
    // 操作按钮信号：MainWidget 接收后汇总配置并下发给 SerialWorker
    void openComClicked();       // 请求打开串口（当前未打开时）
    void closeComClicked();      // 请求关闭串口（当前已打开时）
    void startCollectClicked();  // 请求开始采集（当前未采集时）
    void stopCollectClicked();   // 请求停止采集（当前已采集时）
    void exportCsvClicked();     // 请求导出采集数据为 CSV
    void screenshotClicked();    // 请求保存界面截图
    void curvePauseToggled(bool paused);  // 请求冻结/恢复曲线重绘（不影响采集）

private slots:
    void onPushButtonOpenComClicked();
    void onPushButtonStartClicked();
    void onPushButtonPauseClicked();
    void onPushButtonExportCsvClicked();
    void onPushButtonScreenshotClicked();

private:
    Ui::operationWidget *ui;

    bool m_connected = false;   // 串口是否已打开（维护按钮状态）
    bool m_polling = false;     // 是否正在采集（维护按钮状态）
    bool m_curvePaused = false; // 曲线是否处于冻结状态
};

#endif // OPERATIONWIDGET_H
