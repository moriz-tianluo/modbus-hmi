#ifndef REALCURVEWIDGET_H
#define REALCURVEWIDGET_H

#include "base/BaseWidget.h"

#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

namespace Ui {
class RealCurveWidget;
}

class RealCurveWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit RealCurveWidget(QWidget *parent = nullptr);
    ~RealCurveWidget();

    void languageChange() override;

private:
    // 初始化曲线图：坐标轴、网格、四条曲线与上限线
    void initChart();

    // 创建一条曲线并挂载到坐标轴上
    void addSeries(QChart *chart, QCategoryAxis *axisX, QValueAxis *axisY,
                   const QColor &color, double base, double amplitude, double phase);

    Ui::RealCurveWidget *ui;
};

#endif // REALCURVEWIDGET_H
