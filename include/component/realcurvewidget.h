#ifndef REALCURVEWIDGET_H
#define REALCURVEWIDGET_H

#include "base/BaseWidget.h"

#include "qcustomplot.h"

#include <QElapsedTimer>
#include <QVector>

namespace Ui {
class RealCurveWidget;
}

class ChannelModel;

// 实时曲线卡片（QCustomPlot 版本）：
//   - 横轴：相对时间（自控件创建起经过的秒数），滚动窗口时长可配置（默认 5 分钟）
//   - 纵轴：归一化量程 0~100%（对应 ChannelItem::percent()）
//   - 四条通道曲线（温度/压力/流量/液位）+ 80% 上限参考线
// MainWidget 把 ChannelModel::channelValueChanged 连接到本类槽，实时追加数据点
class RealCurveWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit RealCurveWidget(QWidget *parent = nullptr);
    ~RealCurveWidget();

    void languageChange() override;

    // 绑定通道模型：曲线取值改用 ChannelItem::percent()（0~100），与纵轴量程一致
    void setChannelModel(ChannelModel *model);

    // 曲线保留时长（秒）。缩小时立即裁掉超出的旧点，放大只能等待新数据填充
    void setTimeWindowSeconds(int seconds);
    int timeWindowSeconds() const;

    // 曲线线宽（像素）
    void setLineWidth(int width);
    int lineWidth() const;
    // 上限参考线：是否显示 + 纵轴坐标（0~100 归一化）
    void setUpperLimitVisible(bool visible);
    void setUpperLimitValue(double value);

    // 通道值变化槽：根据 index 找到对应 graph（最多前 4 条），
    // 把 percent 值追加到时间序列，按当前窗口裁剪旧点，重绘由 endRoundUpdate 统一触发
public slots:
    void onChannelValueChanged(int index, double value);

    // 冻结/恢复曲线重绘：冻结期间数据仍继续追加与裁剪，仅不刷新画面
    void setPaused(bool paused);

    // ChannelModel::channelsChanged 槽——通道名称/数量变化后重建图例
    void onChannelsChanged();
    // ChannelModel::channelEnabledChanged 槽——禁用通道隐藏曲线并置灰图例
    void onChannelEnabledChanged(int index, bool enabled);
    // 高亮某条曲线（实时数据表选中行联动），-1 恢复全部常态
    void setHighlightedChannel(int index);

    // 一轮采集的所有通道数据分发完毕后调用：合并本轮 replot，避免每通道重绘一次
    void endRoundUpdate();

    // ---- 曲线导出 ----
    // 保存曲线截图为 PNG
    bool exportScreenshot(const QString &filePath);
    // 导出当前曲线缓冲区为 CSV（长表：时间/通道/百分比/工程值）
    bool exportCurveCsv(const QString &filePath);

signals:
    // 暂停状态变化（本卡片按钮或被 MainWidget 调用后统一广播）
    void pauseStateChanged(bool paused);
    // 点击图例选中通道，用于反选实时数据表对应行
    void channelSelected(int index);

protected:
    // 图例标签点击：转发为 channelSelected
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onPushButtonPauseClicked();
    void onTimeRangeChanged(int index);
    // 用户滚轮缩放/拖动导致横轴范围变化：暂停自动跟随最新
    void onAxisXRangeChanged(const QCPRange &newRange);
    // 双击图表：恢复自动跟随最新数据
    void onPlotDoubleClicked(QMouseEvent *event);
    // 鼠标移动：十字光标读数
    void onPlotMouseMove(QMouseEvent *event);

private:
    // 初始化图表：坐标轴配置、多条 graph、上限参考线、副轴与十字光标
    void initPlot();
    // 初始化图例：复用 .ui 中前 4 个标签，通道超过 4 个时动态补充
    void initLegend();
    // 初始化十字光标（竖线 + 读数文本框）
    void initCrosshair();
    // 刷新右侧工程量副轴：以高亮通道（或首个启用通道）的量程做刻度
    void refreshEngineeringAxis();
    // 百分比 → 工程值
    double percentToEngineering(int channelIndex, double percent) const;
    // 隐藏十字光标
    void hideCrosshair();
    // 创建一条 graph 并设置颜色/线型/数据缓冲
    QCPGraph *addGraph(const QColor &color, const QPen &pen) const;
    // 生成一条正弦示例曲线，便于设计阶段预览
    void demoSineData();
    // 按 m_paused 刷新暂停按钮文字
    void updatePauseButtonText();
    // 按通道模型刷新图例文字、颜色与可见性
    void refreshLegend();
    // 按启用状态/高亮状态刷新 4 条曲线画笔
    void applyChannelStyles();
    // 图例标签 → 通道索引，未命中返回 -1
    int legendIndexOf(const QObject *watched) const;
    // 按当前窗口长度切换横轴刻度格式（<1h 用 分:秒，≥1h 用 时:分）
    void refreshAxisTicker();
    // 让时间范围下拉框与 m_timeWindowSeconds 保持一致（不触发信号）
    void syncTimeRangeCombo();
    // 把上限参考线与标签移动到当前横轴范围
    void refreshLimitLine();
    // 切换某条曲线的图例隐藏/显示状态
    void toggleGraphHidden(int index);

    Ui::RealCurveWidget *ui;

    // 曲线数组（最多 8 条，对应 ChannelModel 前 8 个通道）
    static constexpr int kGraphCount = 8;
    QCPGraph *m_graphs[kGraphCount] = {};
    QCPItemStraightLine *m_limitLine = nullptr;  // 上限参考线
    QCPItemText *m_limitLabel = nullptr;         // 上限参考线右侧文字

    // 图例：前 4 个来自 .ui，后 4 个在 initLegend() 中动态创建
    QLabel *m_legendLabels[kGraphCount] = {};
    QLabel *m_legendDashes[kGraphCount] = {};

    // 十字光标：竖直参考线 + 左上角读数
    QCPItemStraightLine *m_crosshairLine = nullptr;
    QCPItemText *m_crosshairText = nullptr;
    QCPAxis *m_engineeringAxis = nullptr;        // 右侧工程量副轴

    // 横轴显示的时间窗口（秒），运行期可调；超过此值的旧点在 append 时被移除
    static constexpr int kMinTimeWindowSeconds = 30;     // 最短 30 秒
    static constexpr int kMaxTimeWindowSeconds = 14400;  // 最长 4 小时

    // 相对时间基准：横轴使用“自控件创建起经过的秒数”，供 QCPAxisTickerTime 生成刻度
    QElapsedTimer m_elapsed;
    // 当前相对时间（秒）
    double elapsedSeconds() const;

    ChannelModel *m_channelModel = nullptr;  // 用于取 percent() 归一化值
    bool m_hasRealData = false;              // 首个真实数据点到达时清空演示曲线
    bool m_paused = false;                   // 曲线是否处于冻结重绘状态
    int m_highlightedIndex = -1;             // 高亮通道索引，-1 表示无高亮
    double m_timeWindowSeconds = 300.0;      // 曲线保留时长（秒），默认 5 分钟
    bool m_replotPending = false;            // 本轮有数据待重绘（由 endRoundUpdate 消费）
    bool m_followLatest = true;              // 横轴是否自动滚动到最新时刻
    bool m_updatingXRange = false;           // 程序内设置横轴范围期间屏蔽“用户缩放”判定

    int m_lineWidth = 2;                     // 曲线线宽（像素）
    double m_upperLimitValue = 80.0;         // 上限参考线纵轴坐标
    bool m_upperLimitVisible = true;         // 上限参考线是否显示
    bool m_graphHidden[kGraphCount] = {};     // 图例双击隐藏状态，true 表示已隐藏
};

#endif // REALCURVEWIDGET_H
