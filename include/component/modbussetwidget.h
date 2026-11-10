#ifndef MODBUSSETWIDGET_H
#define MODBUSSETWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class ModbusSetWidget;
}

class ModbusSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ModbusSetWidget(QWidget *parent = nullptr);
    ~ModbusSetWidget();

    void languageChange() override;

private:
    Ui::ModbusSetWidget *ui;
};

#endif // MODBUSSETWIDGET_H
