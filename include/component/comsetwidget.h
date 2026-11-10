#ifndef COMSETWIDGET_H
#define COMSETWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class ComSetWidget;
}

class ComSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ComSetWidget(QWidget *parent = nullptr);
    ~ComSetWidget();

    void languageChange() override;

private:
    Ui::ComSetWidget *ui;
};

#endif // COMSETWIDGET_H
