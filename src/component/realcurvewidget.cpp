#include "realcurvewidget.h"
#include "ui_realcurvewidget.h"
#include "Language.h"
#include "common/channelmodel.h"

#include <QDateTime>
#include <QEvent>
#include <QBoxLayout>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPen>
#include <QSignalBlocker>
#include <QTextStream>

namespace {
// 曲线数量上限（前 4 条沿用 .ui 图例，后 4 条动态补充）
constexpr int kMaxGraphs = 8;

// 曲线颜色，与 color.ini 中的 $warningRed / $primaryBlue / $btnGreenBg / $purpleColor 保持一致
const QColor kColorTemp(QStringLiteral("#E0483F"));
const QColor kColorPress(QStringLiteral("#1F6FEB"));
const QColor kColorFlow(QStringLiteral("#15A34A"));
const QColor kColorLevel(QStringLiteral("#8B5CF6"));
const QColor kColorGrid(QStringLiteral("#E9EDF5"));
const QColor kColorAxisText(QStringLiteral("#9AA4B4"));
const QColor kColorBg(QStringLiteral("#FFFFFF"));

// 8 条通道的调色板（超过 4 个通道时继续使用）
const QColor kGraphColors[kMaxGraphs] = {
    kColorTemp,
    kColorPress,
    kColorFlow,
    kColorLevel,
    QColor(QStringLiteral("#F59E0B")),
    QColor(QStringLiteral("#06B6D4")),
    QColor(QStringLiteral("#EC4899")),
    QColor(QStringLiteral("#84CC16")),
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

    // ---- 通道曲线（最多 8 条，超出部分隐藏）----
    for (int i = 0; i < kMaxGraphs; ++i) {
        QPen pen(kGraphColors[i]);
        pen.setWidthF(1.6);
        m_graphs[i] = addGraph(kGraphColors[i], pen);
    }
    initLegend();

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

    // ---- 右侧工程量副轴：坐标位置与主纵轴一致（0~100），刻度文字换成工程值 ----
    m_engineeringAxis = plot->axisRect()->axis(QCPAxis::atRight);
    m_engineeringAxis->setBasePen(QPen(kColorGrid));
    m_engineeringAxis->setTickPen(QPen(kColorGrid));
    m_engineeringAxis->setSubTickPen(QPen(kColorGrid));
    m_engineeringAxis->setTickLabelColor(kColorAxisText);
    m_engineeringAxis->setTickLabelFont(QFont(QString(), 8));
    m_engineeringAxis->setLabelColor(kColorAxisText);
    m_engineeringAxis->setLabelFont(QFont(QString(), 8));
    m_engineeringAxis->setVisible(false);

    initCrosshair();
    ui->chartView->installEventFilter(this);

    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    // 量程已归一化到 0~100，纵轴固定不参与拖动/缩放；横轴仍支持缩放与拖动
    plot->axisRect()->setRangeDrag(Qt::Horizontal);
    plot->axisRect()->setRangeZoom(Qt::Horizontal);

    // 用户手动缩放/拖动横向视窗后暂停自动跟随，双击恢复
    connect(axisX, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this, &RealCurveWidget::onAxisXRangeChanged);
    connect(plot, &QCustomPlot::mouseDoubleClick,
            this, &RealCurveWidget::onPlotDoubleClicked);
    connect(plot, &QCustomPlot::mouseMove,
            this, &RealCurveWidget::onPlotMouseMove);

    plot->setNoAntialiasingOnDrag(false);
    plot->replot();
}

void RealCurveWidget::initLegend()
{
    QLabel *staticLabels[4] = {
        ui->labelLegendTemp, ui->labelLegendPress,
        ui->labelLegendFlow, ui->labelLegendLevel
    };
    QLabel *staticDashes[4] = {
        ui->labelLegendDashTemp, ui->labelLegendDashPress,
        ui->labelLegendDashFlow, ui->labelLegendDashLevel
    };

    for (int index = 0; index < 4; ++index) {
        m_legendLabels[index] = staticLabels[index];
        m_legendDashes[index] = staticDashes[index];
        m_legendLabels[index]->installEventFilter(this);
        m_legendLabels[index]->setCursor(Qt::PointingHandCursor);
    }

    // 第 5~8 通道：动态补充图例，插入到弹性占位符之前（即全部图例之后）
    auto *layout = qobject_cast<QBoxLayout *>(ui->horizontalLayoutTitle);
    if (layout == nullptr) {
        return;
    }
    int insertIndex = layout->count();
    for (int itemIndex = 0; itemIndex < layout->count(); ++itemIndex) {
        QLayoutItem *item = layout->itemAt(itemIndex);
        if (item != nullptr && item->spacerItem() != nullptr
            && item->spacerItem()->expandingDirections() != Qt::Orientations()) {
            insertIndex = itemIndex;
            break;
        }
    }

    for (int index = 4; index < kMaxGraphs; ++index) {
        auto *dash = new QLabel(this);
        dash->setFixedSize(12, 2);
        auto *label = new QLabel(this);
        label->setProperty("role", QStringLiteral("legend"));
        label->installEventFilter(this);
        label->setCursor(Qt::PointingHandCursor);

        layout->insertWidget(insertIndex, dash);
        layout->insertWidget(insertIndex + 1, label);
        insertIndex += 2;

        dash->setVisible(false);
        label->setVisible(false);
        m_legendDashes[index] = dash;
        m_legendLabels[index] = label;
    }
}

void RealCurveWidget::initCrosshair()
{
    QCustomPlot *plot = ui->chartView;

    m_crosshairLine = new QCPItemStraightLine(plot);
    m_crosshairLine->setPen(QPen(kColorAxisText, 1.0, Qt::DashLine));
    m_crosshairLine->point1->setCoords(0, 0);
    m_crosshairLine->point2->setCoords(0, 100);
    m_crosshairLine->setVisible(false);

    m_crosshairText = new QCPItemText(plot);
    m_crosshairText->setClipToAxisRect(true);
    m_crosshairText->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_crosshairText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_crosshairText->position->setCoords(0.012, 0.03);
    m_crosshairText->setFont(QFont(QString(), 7));
    m_crosshairText->setPadding(QMargins(4, 3, 4, 3));
    m_crosshairText->setBrush(QBrush(QColor(255, 255, 255, 225)));
    m_crosshairText->setPen(QPen(kColorGrid));
    m_crosshairText->setColor(QColor(QStringLiteral("#334155")));
    m_crosshairText->setVisible(false);
}

void RealCurveWidget::refreshEngineeringAxis()
{
    if (m_engineeringAxis == nullptr) {
        return;
    }

    if (m_channelModel == nullptr || m_channelModel->channelCount() == 0) {
        m_engineeringAxis->setVisible(false);
        return;
    }

    // 优先用高亮通道的量程，否则取第一个启用通道
    int index = m_highlightedIndex;
    if (index < 0 || index >= m_channelModel->channelCount()) {
        index = -1;
        for (int channel = 0; channel < m_channelModel->channelCount(); ++channel) {
            if (m_channelModel->channelAt(channel).enabled) {
                index = channel;
                break;
            }
        }
    }
    if (index < 0) {
        m_engineeringAxis->setVisible(false);
        return;
    }

    const ChannelItem &item = m_channelModel->channelAt(index);
    QSharedPointer<QCPAxisTickerText> ticker(new QCPAxisTickerText);
    for (int percent = 0; percent <= 100; percent += 25) {
        ticker->addTick(percent, QString::number(percentToEngineering(index, percent),
                                                 'f', item.decimals));
    }

    m_engineeringAxis->setRange(0, 100);
    m_engineeringAxis->setTicker(ticker);
    m_engineeringAxis->setVisible(true);
    m_engineeringAxis->setLabel(item.unit.isEmpty()
                                   ? item.name
                                   : QStringLiteral("%1 (%2)").arg(item.name, item.unit));
}

double RealCurveWidget::percentToEngineering(int channelIndex, double percent) const
{
    if (m_channelModel == nullptr || channelIndex < 0
        || channelIndex >= m_channelModel->channelCount()) {
        return percent;
    }
    const ChannelItem &item = m_channelModel->channelAt(channelIndex);
    return item.rangeMin + (item.rangeMax - item.rangeMin) * percent / 100.0;
}

void RealCurveWidget::hideCrosshair()
{
    if (m_crosshairLine == nullptr || m_crosshairText == nullptr) {
        return;
    }
    if (!m_crosshairLine->visible() && !m_crosshairText->visible()) {
        return;
    }
    m_crosshairLine->setVisible(false);
    m_crosshairText->setVisible(false);
    ui->chartView->replot(QCustomPlot::rpQueuedReplot);
}

void RealCurveWidget::onPlotMouseMove(QMouseEvent *event)
{
    if (m_crosshairLine == nullptr || m_crosshairText == nullptr || m_channelModel == nullptr) {
        return;
    }

    QCPAxis *axisX = ui->chartView->axisRect()->axis(QCPAxis::atBottom);
    const double x = axisX->pixelToCoord(event->pos().x());
    const QCPRange range = axisX->range();
    if (x < range.lower || x > range.upper) {
        hideCrosshair();
        return;
    }

    m_crosshairLine->point1->setCoords(x, 0);
    m_crosshairLine->point2->setCoords(x, 100);
    m_crosshairLine->setVisible(true);

    // 逐通道取光标位置之前最近的数据点，换算成工程值显示
    QStringList lines;
    const int count = qMin(m_channelModel->channelCount(), kMaxGraphs);
    for (int index = 0; index < count; ++index) {
        QCPGraph *graph = m_graphs[index];
        if (graph == nullptr || !graph->visible()) {
            continue;
        }
        const QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(x, true);
        if (it == graph->data()->constEnd()) {
            continue;
        }

        const ChannelItem &item = m_channelModel->channelAt(index);
        const double engineering = percentToEngineering(index, it->value);
        lines << QStringLiteral("%1  %2 %3")
                     .arg(item.name)
                     .arg(engineering, 0, 'f', item.decimals)
                     .arg(item.unit);
    }

    if (lines.isEmpty()) {
        hideCrosshair();
        return;
    }

    m_crosshairText->setText(lines.join(QLatin1Char('\n')));
    m_crosshairText->setVisible(true);
    ui->chartView->replot(QCustomPlot::rpQueuedReplot);
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
    // 副轴切换为高亮通道的工程量刻度
    refreshEngineeringAxis();

    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::refreshLegend()
{
    // 没有模型时保持 .ui 里的静态图例，不做联动
    if (m_channelModel == nullptr) {
        return;
    }

    const int count = m_channelModel->channelCount();
    for (int i = 0; i < kMaxGraphs; ++i) {
        QLabel *label = m_legendLabels[i];
        QLabel *dash = m_legendDashes[i];
        const bool exist = (i < count);

        if (label != nullptr) {
            label->setVisible(exist);
        }
        if (dash != nullptr) {
            dash->setVisible(exist);
            dash->setStyleSheet(QStringLiteral("background-color: %1;")
                                    .arg(kGraphColors[i].name()));
        }

        if (!exist) {
            if (m_graphs[i] != nullptr) {
                m_graphs[i]->setVisible(false);
            }
            continue;
        }

        const ChannelItem &item = m_channelModel->channelAt(i);
        if (label != nullptr) {
            label->setText(item.name);
            // 禁用或图例双击隐藏的通道变灰，与曲线可见性保持一致
            const bool dimmed = !item.enabled || m_graphHidden[i];
            label->setStyleSheet(
                dimmed ? QStringLiteral("color: #B0B8C4;") : QString());
        }
    }

    applyChannelStyles();
    refreshEngineeringAxis();

    if (!m_paused) {
        ui->chartView->replot();
    }
}

void RealCurveWidget::applyChannelStyles()
{
    const int count = (m_channelModel != nullptr) ? m_channelModel->channelCount() : kMaxGraphs;

    for (int i = 0; i < kMaxGraphs; ++i) {
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
    for (int index = 0; index < kMaxGraphs; ++index) {
        if (m_legendLabels[index] == watched) {
            return index;
        }
    }
    return -1;
}

bool RealCurveWidget::eventFilter(QObject *watched, QEvent *event)
{
    // 鼠标移出图表：隐藏十字光标
    if (watched == ui->chartView && event->type() == QEvent::Leave) {
        hideCrosshair();
    }

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

bool RealCurveWidget::exportScreenshot(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }
    return ui->chartView->savePng(filePath, 0, 0, 2.0);
}

bool RealCurveWidget::exportCurveCsv(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << QStringLiteral("time_s,channel,name,percent,engineering,unit\n");

    const int count = (m_channelModel != nullptr) ? m_channelModel->channelCount() : 0;
    for (int index = 0; index < qMin(count, kMaxGraphs); ++index) {
        QCPGraph *graph = m_graphs[index];
        if (graph == nullptr) {
            continue;
        }

        const ChannelItem &item = m_channelModel->channelAt(index);
        QString name = item.name;
        if (name.contains(QLatin1Char(',')) || name.contains(QLatin1Char('"'))) {
            name.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            name = QLatin1Char('"') + name + QLatin1Char('"');
        }

        const QCPGraphDataContainer::const_iterator end = graph->data()->constEnd();
        for (QCPGraphDataContainer::const_iterator it = graph->data()->constBegin();
             it != end; ++it) {
            stream << QString::number(it->key, 'f', 3) << QLatin1Char(',')
                   << index << QLatin1Char(',') << name << QLatin1Char(',')
                   << QString::number(it->value, 'f', 2) << QLatin1Char(',')
                   << QString::number(percentToEngineering(index, it->value),
                                      'f', item.decimals)
                   << QLatin1Char(',') << item.unit << QLatin1Char('\n');
        }
    }
    file.close();
    return true;
}
