#include "realcurvewidget.h"
#include "ui_realcurvewidget.h"
#include "Language.h"
#include "common/channelmodel.h"

#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPen>
#include <QSignalBlocker>

namespace {
// 曲线数量（最多 4 条，对应 ChannelModel 前 4 个通道）
constexpr int kGraphCount = 4;

// 曲线颜色，与 color.ini 中的 $warningRed / $primaryBlue / $btnGreenBg / $purpleColor 保持一致
const QColor kColorTemp(QStringLiteral("#E0483F"));
const QColor kColorPress(QStringLiteral("#1F6FEB"));
const QColor kColorFlow(QStringLiteral("#15A34A"));
const QColor kColorLevel(QStringLiteral("#8B5CF6"));
const QColor kColorGrid(QStringLiteral("#E9EDF5"));
const QColor kColorAxisText(QStringLiteral("#9AA4B4"));
const QColor kColorBg(QStringLiteral("#FFFFFF"));

// 四条通道的颜色，与图例 Dash 控件对应的颜色一致
const QColor kGraphColors[kGraphCount] = {
    kColorTemp,
    kColorPress,
    kColorFlow,
    kColorLevel,
};
} // namespace

RealCurveWidget::RealCurveWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::RealCurveWidget)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    m_elapsed.start();

    initPlot();
    demoSineData();  // 设计阶段示例数据，MainWidget 开始采集后被真实数据覆盖

    connect(ui->pushButtonPause, &QPushButton::clicked,
            this, &RealCurveWidget::onPushButtonPauseClicked);

    // 图例可点击：点哪个通道就选中实时数据表对应行
    QLabel *legendLabels[kGraphCount] = {
        ui->labelLegendTemp, ui->labelLegendPress,
        ui->labelLegendFlow, ui->labelLegendLevel
    };
    for (QLabel *label : legendLabels) {
        label->installEventFilter(this);
        label->setCursor(Qt::PointingHandCursor);
    }

    languageChange();

    // 时间范围切换到具体秒数后立即生效
    connect(ui->comboBoxTimeRange, &QComboBox::currentIndexChanged,
            this, &RealCurveWidget::onTimeRangeChanged);
}

RealCurveWidget::~RealCurveWidget()
{
    delete ui;
}

void RealCurveWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_realCurve"));
    ui->labelSubtitle->setText(LANGUAGE("LG_curve_desc"));

    updatePauseButtonText();

    ui->labelLegendTemp->setText(LANGUAGE("LG_curve_legend_temp"));
    ui->labelLegendPress->setText(LANGUAGE("LG_curve_legend_press"));
    ui->labelLegendFlow->setText(LANGUAGE("LG_curve_legend_flow"));
    ui->labelLegendLevel->setText(LANGUAGE("LG_curve_legend_level"));

    // 图例 Dash 方块用曲线颜色背景填充，QSS 中已有 color 属性配置
    ui->labelLegendDashTemp->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(kColorTemp.name()));
    ui->labelLegendDashPress->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(kColorPress.name()));
    ui->labelLegendDashFlow->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(kColorFlow.name()));
    ui->labelLegendDashLevel->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(kColorLevel.name()));

    // 有通道模型时图例文字改用通道实际名称
    refreshLegend();

    // 上限参考线文字随语言刷新
    refreshLimitLine();

    // 时间范围下拉：itemData 存秒数，文字随语言切换
    {
        QSignalBlocker blocker(ui->comboBoxTimeRange);
        ui->comboBoxTimeRange->clear();
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_curve_range_1min"), 60);
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_curve_range_5min"), 300);
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_curve_range_15min"), 900);
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_curve_range_1hour"), 3600);
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_curve_range_4hour"), 14400);
        syncTimeRangeCombo();
    }
    ui->comboBoxTimeRange->setToolTip(LANGUAGE("LG_curve_rangeTip"));
}

void RealCurveWidget::initPlot()
{
    QCustomPlot *plot = ui->chartView;
    plot->setBackground(QBrush(kColorBg));

    QCPAxis *axisX = plot->axisRect()->axis(QCPAxis::atBottom);
    QCPAxis *axisY = plot->axisRect()->axis(QCPAxis::atLeft);

    // ---- 横轴：相对时间（秒），窗口长度运行期可调 ----
    axisX->setRange(-m_timeWindowSeconds, 0);
    axisX->setBasePen(QPen(kColorGrid));
    axisX->setTickPen(QPen(kColorGrid));
    axisX->setSubTickPen(QPen(kColorGrid));
    axisX->setTickLabelColor(kColorAxisText);
    axisX->setTickLabelFont(QFont(QString(), 8));
    axisX->grid()->setVisible(true);
    axisX->grid()->setPen(QPen(kColorGrid, 1, Qt::DashLine));
    axisX->grid()->setSubGridVisible(false);

    // 刻度格式随窗口长度切换：分钟级用 分:秒，小时级用 时:分
    refreshAxisTicker();

    // ---- 纵轴：归一化量程 0~100% ----
    axisY->setRange(0, 100);
    axisY->setBasePen(QPen(kColorGrid));
    axisY->setTickPen(QPen(kColorGrid));
    axisY->setSubTickPen(QPen(kColorGrid));
    axisY->setTickLabelColor(kColorAxisText);
    axisY->setTickLabelFont(QFont(QString(), 8));
    axisY->grid()->setVisible(true);
    axisY->grid()->setPen(QPen(kColorGrid, 1, Qt::DashLine));
    axisY->grid()->setSubGridVisible(false);

    QSharedPointer<QCPAxisTicker> tickerY(new QCPAxisTicker);
    tickerY->setTickCount(5);
    axisY->setTicker(tickerY);

    // ---- 四条通道曲线 ----
    for (int i = 0; i < kGraphCount; ++i) {
        QPen pen(kGraphColors[i]);
        pen.setWidthF(1.6);
        m_graphs[i] = addGraph(kGraphColors[i], pen);
    }

    // ---- 上限参考线（横轴相对时间坐标）+ 右侧文字标签 ----
    m_limitLine = new QCPItemStraightLine(plot);
    m_limitLine->setPen(QPen(kColorTemp, 1.2, Qt::DashLine));
    m_limitLine->point1->setCoords(-m_timeWindowSeconds, m_upperLimitValue);
    m_limitLine->point2->setCoords(0, m_upperLimitValue);

    m_limitLabel = new QCPItemText(plot);
    m_limitLabel->setClipToAxisRect(false);
    m_limitLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_limitLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_limitLabel->position->setCoords(0.99, m_upperLimitValue / 100.0);
    m_limitLabel->setColor(kColorTemp);
    m_limitLabel->setFont(QFont(QString(), 7));
    m_limitLabel->setText(LANGUAGE("LG_curve_upperLimit").arg(m_upperLimitValue, 0, 'f', 0));

    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    // 量程已归一化到 0~100，纵轴固定不参与拖动/缩放；横轴仍支持缩放与拖动
    plot->axisRect()->setRangeDrag(Qt::Horizontal);
    plot->axisRect()->setRangeZoom(Qt::Horizontal);

    // 用户手动缩放/拖动横向视窗后暂停自动跟随，双击恢复
    connect(axisX, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this, &RealCurveWidget::onAxisXRangeChanged);
    connect(plot, &QCustomPlot::mouseDoubleClick,
            this, &RealCurveWidget::onPlotDoubleClicked);

    plot->setNoAntialiasingOnDrag(false);
    plot->replot();
}

void RealCurveWidget::setChannelModel(ChannelModel *model)
{
    m_channelModel = model;
    refreshLegend();
}

double RealCurveWidget::elapsedSeconds() const
{
    return static_cast<double>(m_elapsed.elapsed()) / 1000.0;
}

QCPGraph *RealCurveWidget::addGraph(const QColor &color, const QPen &pen) const
{
    QCPGraph *graph = ui->chartView->addGraph();
    graph->setPen(pen);
    graph->setBrush(Qt::NoBrush);
    graph->setLineStyle(QCPGraph::lsLine);
    graph->setScatterStyle(QCPScatterStyle::ssNone);
    graph->setAdaptiveSampling(true);
    graph->setName(color.name());  // 名称设为颜色十六进制（占位，实际图例用 UI label）
    return graph;
}

// 设计阶段示例数据：生成当前窗口时长内的正弦曲线点
void RealCurveWidget::demoSineData()
{
    const int pointCount = 120;  // 每 0.5s 一个点
    const double step = m_timeWindowSeconds / static_cast<double>(pointCount);

    for (int i = 0; i <= pointCount; ++i) {
        const double x = i * step;
        const double t = x;
        // 四条相位/振幅不同的正弦曲线，值域 0~100
        const double y0 = 50.0 + 40.0 * qSin((t + 10.0) / 9.0);
        const double y1 = 35.0 + 22.0 * qSin((t + 22.0) / 9.0);
        const double y2 = 55.0 + 31.0 * qSin((t + 8.0) / 9.0);
        const double y3 = 60.0 + 15.0 * qSin((t + 7.0) / 9.0);

        if (m_graphs[0]) m_graphs[0]->addData(x, y0);
        if (m_graphs[1]) m_graphs[1]->addData(x, y1);
        if (m_graphs[2]) m_graphs[2]->addData(x, y2);
        if (m_graphs[3]) m_graphs[3]->addData(x, y3);
    }

    // 横轴显示 [0, 窗口时长]
    m_updatingXRange = true;
    ui->chartView->axisRect()->axis(QCPAxis::atBottom)->setRange(0, m_timeWindowSeconds);
    m_updatingXRange = false;
    ui->chartView->replot();
}

void RealCurveWidget::onChannelValueChanged(int index, double value)
{
    if (index < 0 || index >= kGraphCount || m_graphs[index] == nullptr) {
        return;
    }

    // 首个真实数据点到达：清空演示正弦曲线，避免与实测数据混在一起
    if (!m_hasRealData) {
        m_hasRealData = true;
        for (QCPGraph *graph : m_graphs) {
            if (graph != nullptr) {
                graph->data()->clear();
            }
        }
    }

    // 曲线纵轴为归一化量程 0~100%，优先取 ChannelModel::percent()；无模型时退回原始值
    double plotValue = value;
    if (m_channelModel != nullptr && index < m_channelModel->channelCount()) {
        plotValue = m_channelModel->channelAt(index).percent();
    }

    const double nowSecs = elapsedSeconds();
    m_graphs[index]->addData(nowSecs, plotValue);

    // 修剪掉超出当前窗口的旧数据点
    m_graphs[index]->data()->removeBefore(nowSecs - m_timeWindowSeconds);

    // 冻结期间只缓存数据，不刷新坐标轴与画面
    if (m_paused) {
        return;
    }

    // 自动跟随时横轴滚动到 [now-window, now]
    if (m_followLatest) {
        m_updatingXRange = true;
        ui->chartView->axisRect()->axis(QCPAxis::atBottom)->setRange(
            nowSecs - m_timeWindowSeconds, nowSecs);
        m_updatingXRange = false;
    }

    // 重绘上限参考线（跟着横轴移动）
    refreshLimitLine();

    // 一轮内所有通道共用一次重绘，由 MainWidget 在分发结束后调用 endRoundUpdate
    m_replotPending = true;
}

void RealCurveWidget::endRoundUpdate()
{
    if (!m_replotPending || m_paused) {
        return;
    }

    m_replotPending = false;
    ui->chartView->replot();
}

void RealCurveWidget::setTimeWindowSeconds(int seconds)
{
    const int clamped = qBound(kMinTimeWindowSeconds, seconds, kMaxTimeWindowSeconds);
    if (qFuzzyCompare(m_timeWindowSeconds, static_cast<double>(clamped))) {
        syncTimeRangeCombo();
        return;
    }

    m_timeWindowSeconds = static_cast<double>(clamped);

    // 缩短窗口时立即裁剪；放大窗口无法恢复已删除的旧点，只能等新数据填充
    const double nowSecs = elapsedSeconds();
    for (QCPGraph *graph : m_graphs) {
        if (graph != nullptr) {
            graph->data()->removeBefore(nowSecs - m_timeWindowSeconds);
        }
    }

    refreshAxisTicker();

    // 切换保留时长视为显式调整窗口：恢复自动跟随最新
    m_followLatest = true;

    // 尚无真实数据时保持演示曲线按 [0, window] 显示
    const double rangeStart = m_hasRealData ? nowSecs - m_timeWindowSeconds : 0.0;
    const double rangeEnd = m_hasRealData ? nowSecs : m_timeWindowSeconds;
    m_updatingXRange = true;
    ui->chartView->axisRect()->axis(QCPAxis::atBottom)->setRange(rangeStart, rangeEnd);
    m_updatingXRange = false;

    refreshLimitLine();

    m_replotPending = false;
    ui->chartView->replot();
    syncTimeRangeCombo();
}

int RealCurveWidget::timeWindowSeconds() const
{
    return static_cast<int>(m_timeWindowSeconds);
}

void RealCurveWidget::setLineWidth(int width)
{
    const int clamped = qBound(1, width, 6);
    if (m_lineWidth == clamped) {
        return;
    }

    m_lineWidth = clamped;
    applyChannelStyles();
    if (!m_paused) {
        ui->chartView->replot();
    }
}

int RealCurveWidget::lineWidth() const
{
    return m_lineWidth;
}

void RealCurveWidget::setUpperLimitVisible(bool visible)
{
    if (m_upperLimitVisible == visible) {
        return;
    }

    m_upperLimitVisible = visible;
    refreshLimitLine();
    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::setUpperLimitValue(double value)
{
    m_upperLimitValue = qBound(0.0, value, 100.0);
    refreshLimitLine();
    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::refreshLimitLine()
{
    const double nowSecs = elapsedSeconds();
    const double rangeStart = m_hasRealData ? nowSecs - m_timeWindowSeconds : 0.0;
    const double rangeEnd = m_hasRealData ? nowSecs : m_timeWindowSeconds;

    if (m_limitLine != nullptr) {
        m_limitLine->point1->setCoords(rangeStart, m_upperLimitValue);
        m_limitLine->point2->setCoords(rangeEnd, m_upperLimitValue);
        m_limitLine->setVisible(m_upperLimitVisible);
    }

    if (m_limitLabel != nullptr) {
        m_limitLabel->position->setCoords(0.99, m_upperLimitValue / 100.0);
        m_limitLabel->setText(
            LANGUAGE("LG_curve_upperLimit").arg(m_upperLimitValue, 0, 'f', 0));
        m_limitLabel->setVisible(m_upperLimitVisible);
    }
}

void RealCurveWidget::toggleGraphHidden(int index)
{
    if (index < 0 || index >= kGraphCount) {
        return;
    }

    // 没有对应通道的图例位置不响应
    if (m_channelModel == nullptr || index >= m_channelModel->channelCount()) {
        return;
    }

    m_graphHidden[index] = !m_graphHidden[index];
    applyChannelStyles();
    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::onTimeRangeChanged(int index)
{
    // itemData 存的是窗口秒数
    const int seconds = ui->comboBoxTimeRange->itemData(index).toInt();
    if (seconds > 0) {
        setTimeWindowSeconds(seconds);
    }
}

void RealCurveWidget::onAxisXRangeChanged(const QCPRange &newRange)
{
    Q_UNUSED(newRange);

    // 程序内滚动视窗不算用户操作；用户滚轮缩放/拖动后停止自动跟随
    if (m_updatingXRange) {
        return;
    }
    m_followLatest = false;
}

void RealCurveWidget::onPlotDoubleClicked(QMouseEvent *event)
{
    Q_UNUSED(event);

    // 双击图表恢复自动跟随，并立即回到最新窗口
    m_followLatest = true;

    const double nowSecs = elapsedSeconds();
    m_updatingXRange = true;
    ui->chartView->axisRect()->axis(QCPAxis::atBottom)->setRange(
        nowSecs - m_timeWindowSeconds, nowSecs);
    m_updatingXRange = false;

    ui->chartView->replot();
}

void RealCurveWidget::syncTimeRangeCombo()
{
    QSignalBlocker blocker(ui->comboBoxTimeRange);
    const int index = ui->comboBoxTimeRange->findData(timeWindowSeconds());
    if (index >= 0) {
        ui->comboBoxTimeRange->setCurrentIndex(index);
    }
}

void RealCurveWidget::refreshAxisTicker()
{
    QCPAxis *axisX = ui->chartView->axisRect()->axis(QCPAxis::atBottom);
    QSharedPointer<QCPAxisTickerTime> tickerX(new QCPAxisTickerTime);
    if (m_timeWindowSeconds >= 3600.0) {
        // 小时级窗口用 时:分，避免 %m:%s 在超过 1 小时后刻度含义失真
        tickerX->setTimeFormat(QStringLiteral("%h:%m"));
    } else {
        tickerX->setTimeFormat(QStringLiteral("%m:%s"));
        tickerX->setFieldWidth(QCPAxisTickerTime::tuSeconds, 2);
    }
    tickerX->setTickCount(5);
    axisX->setTicker(tickerX);
}

void RealCurveWidget::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }

    m_paused = paused;
    updatePauseButtonText();

    // 恢复时补一次重绘，把冻结期间累积的数据画出来
    if (!m_paused) {
        // 自动跟随时把视窗拉回最新，避免冻结期间的旧视窗
        if (m_followLatest) {
            const double nowSecs = elapsedSeconds();
            m_updatingXRange = true;
            ui->chartView->axisRect()->axis(QCPAxis::atBottom)->setRange(
                nowSecs - m_timeWindowSeconds, nowSecs);
            m_updatingXRange = false;
        }
        m_replotPending = false;
        ui->chartView->replot();
    }

    emit pauseStateChanged(m_paused);
}

void RealCurveWidget::onPushButtonPauseClicked()
{
    setPaused(!m_paused);
}

void RealCurveWidget::updatePauseButtonText()
{
    ui->pushButtonPause->setText(
        m_paused ? LANGUAGE("LG_curve_btnResume") : LANGUAGE("LG_curve_btnPause"));
}

// ============== 与通道配置 / 实时数据的联动 ==============

void RealCurveWidget::onChannelsChanged()
{
    // 通道名称、数量变化后重建图例，并让被禁用通道重新计算可见性
    refreshLegend();
}

void RealCurveWidget::onChannelEnabledChanged(int index, bool enabled)
{
    Q_UNUSED(index);
    Q_UNUSED(enabled);

    // 启用状态变化会同时影响图例置灰与曲线可见性
    refreshLegend();
}

void RealCurveWidget::setHighlightedChannel(int index)
{
    if (m_highlightedIndex == index) {
        return;
    }

    // 越界一律视为取消高亮
    if (m_channelModel == nullptr || index < 0 || index >= kGraphCount
        || index >= m_channelModel->channelCount()) {
        index = -1;
    }

    m_highlightedIndex = index;
    applyChannelStyles();

    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::refreshLegend()
{
    QLabel *legendLabels[kGraphCount] = {
        ui->labelLegendTemp, ui->labelLegendPress,
        ui->labelLegendFlow, ui->labelLegendLevel
    };
    QLabel *legendDashes[kGraphCount] = {
        ui->labelLegendDashTemp, ui->labelLegendDashPress,
        ui->labelLegendDashFlow, ui->labelLegendDashLevel
    };

    // 没有模型时保持 .ui 里的静态图例，不做联动
    if (m_channelModel == nullptr) {
        return;
    }

    const int count = m_channelModel->channelCount();
    for (int i = 0; i < kGraphCount; ++i) {
        const bool exist = (i < count);
        legendLabels[i]->setVisible(exist);
        legendDashes[i]->setVisible(exist);

        if (!exist) {
            if (m_graphs[i] != nullptr) {
                m_graphs[i]->setVisible(false);
            }
            continue;
        }

        const ChannelItem &item = m_channelModel->channelAt(i);
        legendLabels[i]->setText(item.name);
        // 禁用或图例双击隐藏的通道变灰，与曲线可见性保持一致
        const bool dimmed = !item.enabled || m_graphHidden[i];
        legendLabels[i]->setStyleSheet(
            dimmed ? QStringLiteral("color: #B0B8C4;") : QString());
    }

    applyChannelStyles();

    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::applyChannelStyles()
{
    const int count = (m_channelModel != nullptr) ? m_channelModel->channelCount() : kGraphCount;

    for (int i = 0; i < kGraphCount; ++i) {
        QCPGraph *graph = m_graphs[i];
        if (graph == nullptr) {
            continue;
        }

        const bool exist = (i < count);
        const bool enabled = exist
            && (m_channelModel == nullptr || m_channelModel->channelAt(i).enabled);
        const bool highlighted = (m_highlightedIndex == i);
        const bool dimmed = (m_highlightedIndex >= 0) && !highlighted;

        // 高亮线加粗，其余通道降低透明度；禁用通道进一步淡化
        QColor color = kGraphColors[i];
        if (exist && m_channelModel != nullptr
            && m_channelModel->channelAt(i).color.isValid()) {
            // 通道配置了自定义颜色时优先使用
            color = m_channelModel->channelAt(i).color;
        }
        if (dimmed) {
            color.setAlpha(90);
        } else if (!enabled) {
            color.setAlpha(110);
        }

        QPen pen(color);
        pen.setWidthF(highlighted ? (m_lineWidth + 1.0) : m_lineWidth);
        graph->setPen(pen);
        // 图例双击隐藏的曲线不参与显示
        graph->setVisible(enabled && !m_graphHidden[i]);
    }
}

int RealCurveWidget::legendIndexOf(const QObject *watched) const
{
    if (watched == ui->labelLegendTemp) {
        return 0;
    }
    if (watched == ui->labelLegendPress) {
        return 1;
    }
    if (watched == ui->labelLegendFlow) {
        return 2;
    }
    if (watched == ui->labelLegendLevel) {
        return 3;
    }
    return -1;
}

bool RealCurveWidget::eventFilter(QObject *watched, QEvent *event)
{
    const int index = legendIndexOf(watched);
    if (index >= 0) {
        // 双击图例：隐藏/显示该通道曲线（单击仍为选中联动）
        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleGraphHidden(index);
            return true;
        }

        // QLabel 自身会忽略鼠标事件，这里必须在按下时就消费，否则事件会冒泡到父控件
        if (event->type() == QEvent::MouseButtonPress) {
            emit channelSelected(index);
            return true;
        }
    }

    return BaseWidget::eventFilter(watched, event);
}
