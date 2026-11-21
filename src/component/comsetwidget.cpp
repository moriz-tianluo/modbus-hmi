#include "comsetwidget.h"
#include "ui_comsetwidget.h"
#include "Language.h"

#include <QComboBox>
#include <QSerialPort>

ComSetWidget::ComSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ComSetWidget)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);
    refreshPorts();
    initConnections();
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

void ComSetWidget::refreshPorts()
{
    const QStringList ports = SerialPortHelper::availablePorts();
    ui->comboBoxPort->clear();
    ui->comboBoxPort->addItems(ports);
}

void ComSetWidget::initConnections()
{
    connect(ui->comboBoxPort, &QComboBox::currentIndexChanged,
            this, &ComSetWidget::settingsChanged);
    connect(ui->comboBoxBit, &QComboBox::currentIndexChanged,
            this, &ComSetWidget::settingsChanged);
    connect(ui->comboBoxFrame, &QComboBox::currentIndexChanged,
            this, &ComSetWidget::settingsChanged);
    connect(ui->comboBoxTimeout, &QComboBox::currentIndexChanged,
            this, &ComSetWidget::settingsChanged);
    connect(ui->comboBoxRe, &QComboBox::currentIndexChanged,
            this, &ComSetWidget::settingsChanged);
}

// 把 UI 下拉框字符串解析为 QSerialPort 的枚举值
static QSerialPort::DataBits parseDataBits(const QString &frameText)
{
    // frameText 形如 "8-N-1"，第一个字符就是数据位长度
    if (frameText.isEmpty()) {
        return QSerialPort::Data8;
    }
    const int bits = frameText.at(0).digitValue();
    switch (bits) {
    case 5: return QSerialPort::Data5;
    case 6: return QSerialPort::Data6;
    case 7: return QSerialPort::Data7;
    case 8: default: return QSerialPort::Data8;
    }
}

static QSerialPort::Parity parseParity(const QString &frameText)
{
    // 中间字符：N=无校验, E=偶校验, O=奇校验
    const int dash1 = frameText.indexOf('-');
    const int dash2 = frameText.indexOf('-', dash1 + 1);
    if (dash1 < 0 || dash2 < 0 || dash2 - dash1 != 2) {
        return QSerialPort::NoParity;
    }
    const QChar ch = frameText.at(dash1 + 1);
    switch (ch.toUpper().toLatin1()) {
    case 'E': return QSerialPort::EvenParity;
    case 'O': return QSerialPort::OddParity;
    case 'M': return QSerialPort::MarkParity;
    case 'S': return QSerialPort::SpaceParity;
    default: return QSerialPort::NoParity;
    }
}

static QSerialPort::StopBits parseStopBits(const QString &frameText)
{
    // 最后字符：1, 1.5, 2
    const int lastDash = frameText.lastIndexOf('-');
    if (lastDash < 0) {
        return QSerialPort::OneStop;
    }
    const QString suffix = frameText.mid(lastDash + 1);
    if (suffix == QStringLiteral("2")) {
        return QSerialPort::TwoStop;
    }
    if (suffix.contains(QLatin1Char('.')) || suffix == QStringLiteral("1.5")) {
        return QSerialPort::OneAndHalfStop;
    }
    return QSerialPort::OneStop;
}

SerialPortConfig ComSetWidget::getSerialPortConfig() const
{
    SerialPortConfig config;

    // 端口名
    config.portName = ui->comboBoxPort->currentText();

    // 波特率（字符串转整数）
    bool ok = false;
    const qint32 baud = ui->comboBoxBit->currentText().toInt(&ok);
    config.baudRate = ok ? baud : 9600;

    // 帧格式 → 数据位/校验位/停止位
    const QString frameText = ui->comboBoxFrame->currentText();
    config.dataBits = parseDataBits(frameText);
    config.parity   = parseParity(frameText);
    config.stopBits = parseStopBits(frameText);
    config.flowControl = QSerialPort::NoFlowControl;

    return config;
}

int ComSetWidget::currentResponseTimeoutMs() const
{
    // 下拉项为 "500 ms" 形式，取出数字部分；解析失败按 500ms 兜底
    const QString text = ui->comboBoxTimeout->currentText();
    bool ok = false;
    const int timeoutMs = text.split(QLatin1Char(' ')).constFirst().toInt(&ok);
    return ok ? timeoutMs : 500;
}

int ComSetWidget::currentRetryCount() const
{
    bool ok = false;
    const int retries = ui->comboBoxRe->currentText().toInt(&ok);
    return ok ? retries : 3;
}

QString ComSetWidget::frameTextOf(const SerialPortConfig &config) const
{
    // 数据位：8 / 7 / 6 / 5
    QString parityText = QStringLiteral("N");
    switch (config.parity) {
    case QSerialPort::EvenParity:  parityText = QStringLiteral("E"); break;
    case QSerialPort::OddParity:   parityText = QStringLiteral("O"); break;
    default:                       parityText = QStringLiteral("N"); break;
    }

    QString stopText = QStringLiteral("1");
    if (config.stopBits == QSerialPort::TwoStop) {
        stopText = QStringLiteral("2");
    } else if (config.stopBits == QSerialPort::OneAndHalfStop) {
        stopText = QStringLiteral("1.5");
    }

    return QStringLiteral("%1-%2-%3")
        .arg(static_cast<int>(config.dataBits))
        .arg(parityText)
        .arg(stopText);
}

void ComSetWidget::applySettings(const SerialPortConfig &config,
                                 int responseTimeoutMs, int retryCount)
{
    // 端口：记忆的端口当前不存在时临时补入下拉框，避免配置丢失
    if (!config.portName.isEmpty()) {
        int portIndex = ui->comboBoxPort->findText(config.portName);
        if (portIndex < 0) {
            ui->comboBoxPort->addItem(config.portName);
            portIndex = ui->comboBoxPort->count() - 1;
        }
        ui->comboBoxPort->setCurrentIndex(portIndex);
    }

    const int baudIndex = ui->comboBoxBit->findText(QString::number(config.baudRate));
    if (baudIndex >= 0) {
        ui->comboBoxBit->setCurrentIndex(baudIndex);
    }

    const int frameIndex = ui->comboBoxFrame->findText(frameTextOf(config));
    if (frameIndex >= 0) {
        ui->comboBoxFrame->setCurrentIndex(frameIndex);
    }

    // 超时下拉项形如 "500 ms"，按数字部分匹配
    for (int i = 0; i < ui->comboBoxTimeout->count(); ++i) {
        bool ok = false;
        const int value = ui->comboBoxTimeout->itemText(i)
                              .split(QLatin1Char(' ')).constFirst().toInt(&ok);
        if (ok && value == responseTimeoutMs) {
            ui->comboBoxTimeout->setCurrentIndex(i);
            break;
        }
    }

    const int retryIndex = ui->comboBoxRe->findText(QString::number(retryCount));
    if (retryIndex >= 0) {
        ui->comboBoxRe->setCurrentIndex(retryIndex);
    }
}
