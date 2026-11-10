#include "modbussetwidget.h"
#include "ui_modbussetwidget.h"
#include "Language.h"

ModbusSetWidget::ModbusSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ModbusSetWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
}

ModbusSetWidget::~ModbusSetWidget()
{
    delete ui;
}

void ModbusSetWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_modbusSet"));
    ui->labelFuncCode->setText(LANGUAGE("LG_modbus_funcCode"));
    ui->labelStartAddr->setText(LANGUAGE("LG_modbus_startAddr"));
    ui->labelQuantity->setText(LANGUAGE("LG_modbus_quantity"));
    ui->labelTip->setText(LANGUAGE("LG_modbus_tip"));
}
