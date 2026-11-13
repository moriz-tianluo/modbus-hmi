#include "operationwidget.h"
#include "ui_operationwidget.h"
#include "Language.h"

operationWidget::operationWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::operationWidget)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    // 按钮槽
    connect(ui->pushButtonOpenCom, &QPushButton::clicked,
            this, &operationWidget::onPushButtonOpenComClicked);
    connect(ui->pushButtonStart, &QPushButton::clicked,
            this, &operationWidget::onPushButtonStartClicked);
    connect(ui->pushButtonPause, &QPushButton::clicked,
            this, &operationWidget::onPushButtonPauseClicked);
    connect(ui->pushButtonExportCsv, &QPushButton::clicked,
            this, &operationWidget::onPushButtonExportCsvClicked);
    connect(ui->pushButtonScreenshot, &QPushButton::clicked,
            this, &operationWidget::onPushButtonScreenshotClicked);

    languageChange();
    updateConnectionState(false);
    updatePollingState(false);
    updateCurvePauseState(false);
}

operationWidget::~operationWidget()
{
    delete ui;
}

void operationWidget::languageChange()
{
    ui->pushButtonExportCsv->setText(LANGUAGE("LG_tool_exportCsv"));
    ui->pushButtonScreenshot->setText(LANGUAGE("LG_tool_screenshot"));

    // 按钮文字由 updateConnectionState / updatePollingState 维护
    updateConnectionState(m_connected);
    updatePollingState(m_polling);
    updateCurvePauseState(m_curvePaused);
}

void operationWidget::updateConnectionState(bool opened)
{
    m_connected = opened;
    ui->pushButtonOpenCom->setText(
        opened ? LANGUAGE("LG_tool_closeSerial") : LANGUAGE("LG_tool_openSerial"));

    if (opened) {
        setStatusText(QStringLiteral("● ") + LANGUAGE("LG_tool_statusOpened"));
    } else {
        setStatusText(QStringLiteral("○ ") + LANGUAGE("LG_tool_statusClosed"));
    }

    // 未连接时「开始采集」不可用，避免下发必然失败的请求
    ui->pushButtonStart->setEnabled(opened || m_polling);
}

void operationWidget::updatePollingState(bool running)
{
    m_polling = running;
    ui->pushButtonStart->setText(
        running ? LANGUAGE("LG_tool_stopCollect") : LANGUAGE("LG_tool_startCollect"));
    ui->pushButtonStart->setEnabled(running || m_connected);
}

void operationWidget::updateCurvePauseState(bool paused)
{
    m_curvePaused = paused;
    ui->pushButtonPause->setText(
        paused ? LANGUAGE("LG_tool_resumeCurve") : LANGUAGE("LG_tool_pauseCurve"));
}

void operationWidget::setStatusText(const QString &text)
{
    ui->labelStatus->setText(text);
}

void operationWidget::onPushButtonOpenComClicked()
{
    if (m_connected) {
        emit closeComClicked();
    } else {
        emit openComClicked();
    }
}

void operationWidget::onPushButtonStartClicked()
{
    if (m_polling) {
        emit stopCollectClicked();
    } else {
        emit startCollectClicked();
    }
}

void operationWidget::onPushButtonPauseClicked()
{
    // 暂停曲线只冻结重绘，采集继续运行（与「停止采集」是两件事）
    const bool paused = !m_curvePaused;
    updateCurvePauseState(paused);
    emit curvePauseToggled(paused);
}

void operationWidget::onPushButtonExportCsvClicked()
{
    emit exportCsvClicked();
}

void operationWidget::onPushButtonScreenshotClicked()
{
    emit screenshotClicked();
}
