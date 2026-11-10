#include "datasavewidget.h"
#include "ui_datasavewidget.h"
#include "Language.h"

DataSaveWidget::DataSaveWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::DataSaveWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
}

DataSaveWidget::~DataSaveWidget()
{
    delete ui;
}

void DataSaveWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_dataStore"));
    ui->labelDbInfo->setText(LANGUAGE("LG_store_dbInfo"));
    ui->labelRecordCount->setText(LANGUAGE("LG_store_recordCount"));
    ui->pushButtonQuery->setText(LANGUAGE("LG_store_btn_query"));
    ui->pushButtonExport->setText(LANGUAGE("LG_store_btn_export"));

    initTimeRangeItems();
}

void DataSaveWidget::initTimeRangeItems()
{
    const int currentIndex = ui->comboBoxTimeRange->currentIndex();

    ui->comboBoxTimeRange->clear();
    ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_1h"));
    ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_6h"));
    ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_24h"));
    ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_7d"));
    ui->comboBoxTimeRange->setCurrentIndex(currentIndex < 0 ? 0 : currentIndex);
}
