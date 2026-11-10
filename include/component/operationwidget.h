#ifndef OPERATIONWIDGET_H
#define OPERATIONWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class operationWidget;
}

class operationWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit operationWidget(QWidget *parent = nullptr);
    ~operationWidget();

    void languageChange() override;

private:
    Ui::operationWidget *ui;
};

#endif // OPERATIONWIDGET_H
