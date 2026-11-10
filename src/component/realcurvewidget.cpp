#include "realcurvewidget.h"
#include "ui_realcurvewidget.h"
#include "Language.h"

#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>

#include <QPainter>
#include <QPen>
#include <QtMath>

namespace {
// 曲线颜色，与 color.ini 中的 $warningRed / $primaryBlue / $btnGreenBg / $purpleColor 保持一致
const QColor kColorTemp(QStringLiteral("#E0483F"));
const QColor kColorPress(QStringLiteral("#1F6FEB"));
const QColor kColorFlow(QStringLiteral("#15A34A"));
const QColor kColorLevel(QStringLiteral("#8B5CF6"));
const QColor kColorGrid(QStringLiteral("#E9EDF5"));
const QColor kColorAxisText(QStringLiteral("#9AA4B4"));
}

RealCurveWidget::RealCurveWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::RealCurveWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    initChart();
    languageChange();
}

RealCurveWidget::~RealCurveWidget()
{
    delete ui;
}

void RealCurveWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_realCurve"));
    ui->labelSubtitle->setText(LANGUAGE("LG_curve_desc"));
    ui->pushButtonPause->setText(LANGUAGE("LG_curve_btnPause"));

    ui->labelLegendTemp->setText(LANGUAGE("LG_curve_legend_temp"));
    ui->labelLegendPress->setText(LANGUAGE("LG_curve_legend_press"));
    ui->labelLegendFlow->setText(LANGUAGE("LG_curve_legend_flow"));
    ui->labelLegendLevel->setText(LANGUAGE("LG_curve_legend_level"));
}

void RealCurveWidget::initChart()
{
    QChart *chart = new QChart();
    chart->legend()->hide();
    chart->setBackgroundBrush(Qt::white);
    chart->setBackgroundRoundness(0);
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->setMargins(QMargins(2, 2, 2, 2));

    // 横轴：滚动时间窗口（-60s ~ now）
    QCategoryAxis *axisX = new QCategoryAxis();
    axisX->setRange(-60, 0);
    axisX->append(QStringLiteral("-60s"), -60);
    axisX->append(QStringLiteral("-45s"), -45);
    axisX->append(QStringLiteral("-30s"), -30);
    axisX->append(QStringLiteral("-15s"), -15);
    axisX->append(QStringLiteral("now"), 0);
    axisX->setGridLineColor(kColorGrid);
    axisX->setLabelsColor(kColorAxisText);
    axisX->setLinePenColor(kColorGrid);
    axisX->setLabelsFont(QFont(QString(), 8));

    // 纵轴：归一化量程 0 ~ 100 %
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setTickCount(5);
    axisY->setLabelFormat(QStringLiteral("%d"));
    axisY->setGridLineColor(kColorGrid);
    axisY->setLabelsColor(kColorAxisText);
    axisY->setLinePenColor(kColorGrid);
    axisY->setLabelsFont(QFont(QString(), 8));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    // 四条通道曲线
    addSeries(chart, axisX, axisY, kColorTemp, 50, 10, 90);
    addSeries(chart, axisX, axisY, kColorPress, 35, 12, 220);
    addSeries(chart, axisX, axisY, kColorFlow, 55, 8, 310);
    addSeries(chart, axisX, axisY, kColorLevel, 60, 7, 150);

    // 上限参考线（80%）
    QLineSeries *limitSeries = new QLineSeries();
    limitSeries->append(-60, 80);
    limitSeries->append(0, 80);
    QPen limitPen(kColorTemp);
    limitPen.setStyle(Qt::DashLine);
    limitPen.setWidthF(1.2);
    limitSeries->setPen(limitPen);
    chart->addSeries(limitSeries);
    limitSeries->attachAxis(axisX);
    limitSeries->attachAxis(axisY);

    ui->chartView->setChart(chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
    ui->chartView->setBackgroundBrush(Qt::white);
}

// 生成一条按正弦变化的示例曲线，便于设计阶段预览
void RealCurveWidget::addSeries(QChart *chart, QCategoryAxis *axisX, QValueAxis *axisY,
                                const QColor &color, double base, double amplitude, double phase)
{
    QLineSeries *series = new QLineSeries();
    for (int index = 0; index <= 120; ++index) {
        const double x = -60.0 + index * 0.5;
        const double y = base + amplitude * qSin((x + phase) / 9.0);
        series->append(x, y);
    }

    QPen pen(color);
    pen.setWidthF(1.6);
    series->setPen(pen);

    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);
}
