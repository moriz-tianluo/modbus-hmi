#ifndef REALDATAWIDGET_H
#define REALDATAWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class RealDataWidget;
}

class RealDataWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit RealDataWidget(QWidget *parent = nullptr);      
    ~RealDataWidget();

    void languageChange() override;

private:
    // 初始化表格列宽与行高
    void initTable();
    // 设置表头文字
    void setHeaderText(int column, const QString &text);

    static constexpr int kColumnCount = 8;

    Ui::RealDataWidget *ui;
};

#endif // REALDATAWIDGET_H
