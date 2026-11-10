#ifndef ALARMANDFAULTWIDGET_H
#define ALARMANDFAULTWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class AlarmAndFaultWidget;
}

class AlarmAndFaultWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit AlarmAndFaultWidget(QWidget *parent = nullptr);
    ~AlarmAndFaultWidget();

    void languageChange() override;

private:
    Ui::AlarmAndFaultWidget *ui;
};

#endif // ALARMANDFAULTWIDGET_H
