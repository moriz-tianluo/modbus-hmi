#ifndef DATASAVEWIDGET_H
#define DATASAVEWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class DataSaveWidget;
}

class DataSaveWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit DataSaveWidget(QWidget *parent = nullptr);
    ~DataSaveWidget();

    void languageChange() override;

private:
    // 按当前语言重建时间范围下拉项
    void initTimeRangeItems();

    Ui::DataSaveWidget *ui;
};

#endif // DATASAVEWIDGET_H
