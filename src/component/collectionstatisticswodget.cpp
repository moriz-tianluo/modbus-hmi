#include "collectionstatisticswodget.h"
#include "ui_collectionstatisticswodget.h"
#include "Language.h"

CollectionStatisticsWodget::CollectionStatisticsWodget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::CollectionStatisticsWodget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
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
}
