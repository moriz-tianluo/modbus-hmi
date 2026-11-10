#include "operationwidget.h"
#include "ui_operationwidget.h"
#include "Language.h"

operationWidget::operationWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::operationWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
}

operationWidget::~operationWidget()
{
    delete ui;
}

void operationWidget::languageChange()
{
    ui->labelPort->setText(LANGUAGE("LG_tool_port"));
    ui->labelBit->setText(LANGUAGE("LG_tool_baud"));
    ui->labelFrameFormat->setText(LANGUAGE("LG_tool_frame"));
    ui->labelPoll->setText(LANGUAGE("LG_tool_slave"));
    ui->labelPeriod->setText(LANGUAGE("LG_tool_period"));

    ui->pushButtonRefresh->setText(LANGUAGE("LG_tool_refresh"));
    ui->pushButtonOpenCom->setText(LANGUAGE("LG_tool_openSerial"));
    ui->pushButtonStart->setText(LANGUAGE("LG_tool_startCollect"));
    ui->pushButtonPause->setText(LANGUAGE("LG_tool_pauseCurve"));
    ui->pushButtonExportCsv->setText(LANGUAGE("LG_tool_exportCsv"));
    ui->pushButtonScreenshot->setText(LANGUAGE("LG_tool_screenshot"));

    // 运行状态：绿点 + 文字
    ui->labelStatus->setText(QStringLiteral("● ") + LANGUAGE("LG_tool_statusRunning"));
}
