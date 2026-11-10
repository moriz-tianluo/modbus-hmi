#include "channelsetwidget.h"
#include "ui_channelsetwidget.h"
#include "Language.h"

#include <QHeaderView>

ChannelSetWidget::ChannelSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ChannelSetWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    initTable();
    languageChange();
}

ChannelSetWidget::~ChannelSetWidget()
{
    delete ui;
}

void ChannelSetWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_channelConfig"));
    ui->pushButtonSelectAll->setText(LANGUAGE("LG_channel_selectAll"));

    // 表头文字
    setHeaderText(0, LANGUAGE("LG_table_channel"));
    setHeaderText(1, LANGUAGE("LG_table_addr"));
    setHeaderText(2, LANGUAGE("LG_table_curValue"));

    ui->labelBuffer->setText(LANGUAGE("LG_channel_buffer"));
    ui->checkBoxSaveSqlite->setText(LANGUAGE("LG_channel_saveSqlite"));
    ui->pushButtonImportConfig->setText(LANGUAGE("LG_channel_importConfig"));
    ui->pushButtonExportJson->setText(LANGUAGE("LG_channel_exportJson"));
    ui->labelConfigPath->setText(LANGUAGE("LG_channel_configPath"));
    ui->labelChannelSummary->setText(LANGUAGE("LG_channel_statTip"));
}

// 设置指列表头文字
void ChannelSetWidget::setHeaderText(int column, const QString &text)
{
    QTableWidgetItem *item = ui->tableWidgetChannel->horizontalHeaderItem(column);
    if (item == nullptr) {
        ui->tableWidgetChannel->setHorizontalHeaderItem(column, new QTableWidgetItem(text));
        return;
    }
    item->setText(text);
}

// 初始化通道表格的列宽与表头高度
void ChannelSetWidget::initTable()
{
    QTableWidget *table = ui->tableWidgetChannel;
    QHeaderView *header = table->horizontalHeader();

    // 通道列自适应剩余宽度，地址列与当前值列使用固定宽度
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    header->setSectionResizeMode(2, QHeaderView::Fixed);

    table->setColumnWidth(1, 80);
    table->setColumnWidth(2, 80);
    header->setFixedHeight(26);

    table->verticalHeader()->setDefaultSectionSize(28);
}
