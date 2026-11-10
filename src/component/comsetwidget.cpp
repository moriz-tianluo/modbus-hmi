#include "comsetwidget.h"
#include "ui_comsetwidget.h"
#include "Language.h"

ComSetWidget::ComSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ComSetWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
}

ComSetWidget::~ComSetWidget()
{
    delete ui;
}

void ComSetWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_serialSet"));
    ui->labelPort->setText(LANGUAGE("LG_serial_port"));
    ui->labelBit->setText(LANGUAGE("LG_serial_baud"));
    ui->labelFrame->setText(LANGUAGE("LG_serial_frame"));
    ui->labelTimeout->setText(LANGUAGE("LG_serial_timeout"));
    ui->labelRe->setText(LANGUAGE("LG_serial_retry"));
    ui->labelTip->setText(LANGUAGE("LG_serial_frameTip"));
}
