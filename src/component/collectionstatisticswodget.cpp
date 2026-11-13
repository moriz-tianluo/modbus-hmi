#include "collectionstatisticswodget.h"
#include "ui_collectionstatisticswodget.h"
#include "Language.h"

#include <QLocale>

CollectionStatisticsWodget::CollectionStatisticsWodget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::CollectionStatisticsWodget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
    resetStatistics();
}

CollectionStatisticsWodget::~CollectionStatisticsWodget()
{
    delete ui;
}

void CollectionStatisticsWodget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_collectStat"));

    // 统计小格标题
    ui->labelTotalCaption->setText(LANGUAGE("LG_stat_totalReq"));
    ui->labelSuccessCaption->setText(LANGUAGE("LG_stat_successRate"));
    ui->labelTimeoutCaption->setText(LANGUAGE("LG_stat_timeout"));
    ui->labelExceptionCaption->setText(LANGUAGE("LG_stat_exceptionCode"));

    // 统计明细行
    ui->labelAvgRttCaption->setText(LANGUAGE("LG_stat_avgRtt"));
    ui->labelPeakRttCaption->setText(LANGUAGE("LG_stat_peakRtt"));
    ui->labelCrcErrCaption->setText(LANGUAGE("LG_stat_crcErr"));
    ui->labelLostRateCaption->setText(LANGUAGE("LG_stat_lostRate"));

    // 语言切换后数值格式（千分位/单位）也需重新生成
    refreshDisplay();
}

void CollectionStatisticsWodget::resetStatistics()
{
    m_totalRounds = 0;
    m_successRounds = 0;
    m_failedRounds = 0;
    m_timeoutCount = 0;
    m_exceptionCount = 0;
    m_crcErrorCount = 0;
    m_rttSumMs = 0;
    m_rttPeakMs = 0;

    refreshDisplay();
}

void CollectionStatisticsWodget::onRoundSuccess(qint64 rttMs)
{
    ++m_totalRounds;
    ++m_successRounds;

    m_rttSumMs += rttMs;
    if (rttMs > m_rttPeakMs) {
        m_rttPeakMs = rttMs;
    }

    refreshDisplay();
}

void CollectionStatisticsWodget::onRoundFailed(ModbusError error)
{
    ++m_totalRounds;
    ++m_failedRounds;

    // 按本轮最终失败原因分类累计
    switch (error) {
    case ModbusError::Timeout:
        ++m_timeoutCount;
        break;
    case ModbusError::ExceptionCode:
        ++m_exceptionCount;
        break;
    case ModbusError::CrcError:
        ++m_crcErrorCount;
        break;
    default:
        break;
    }

    refreshDisplay();
}

void CollectionStatisticsWodget::refreshDisplay()
{
    const QString percentUnit = LANGUAGE("LG_unit_percent");
    const QString msUnit = LANGUAGE("LG_unit_ms");

    // 总请求（千分位显示）
    ui->labelTotalValue->setText(QLocale().toString(m_totalRounds));

    // 成功率 = 成功轮次 / 总轮次
    const double successRate = m_totalRounds > 0
        ? static_cast<double>(m_successRounds) * 100.0 / static_cast<double>(m_totalRounds)
        : 0.0;
    ui->labelSuccessValue->setText(QString::number(successRate, 'f', 2) + percentUnit);

    // 超时 / 异常码
    ui->labelTimeoutValue->setText(QLocale().toString(m_timeoutCount));
    ui->labelExceptionValue->setText(QLocale().toString(m_exceptionCount));

    // 平均 / 峰值 RTT（无成功轮次时显示 0.0）
    const double avgRtt = m_successRounds > 0
        ? static_cast<double>(m_rttSumMs) / static_cast<double>(m_successRounds)
        : 0.0;
    ui->labelAvgRttValue->setText(QString::number(avgRtt, 'f', 1) + QLatin1Char(' ') + msUnit);
    ui->labelPeakRttValue->setText(QString::number(m_rttPeakMs) + QLatin1Char(' ') + msUnit);

    // CRC 校验错
    ui->labelCrcErrValue->setText(QLocale().toString(m_crcErrorCount));

    // 丢点率 = 失败轮次 / 总轮次
    const double lostRate = m_totalRounds > 0
        ? static_cast<double>(m_failedRounds) * 100.0 / static_cast<double>(m_totalRounds)
        : 0.0;
    ui->labelLostRateValue->setText(QString::number(lostRate, 'f', 2) + percentUnit);
}
