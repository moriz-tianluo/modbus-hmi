#include "realdatawidget.h"
#include "ui_realdatawidget.h"
#include "Language.h"

#include <QHeaderView>

RealDataWidget::RealDataWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::RealDataWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    initTable();
    languageChange();
}

RealDataWidget::~RealDataWidget()
{
    delete ui;
}

void RealDataWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_realData"));
    ui->labelSubtitle->setText(LANGUAGE("LG_realData_desc"));

    setHeaderText(0, LANGUAGE("LG_rd_col_channel"));
    setHeaderText(1, LANGUAGE("LG_rd_col_reg"));
    setHeaderText(2, LANGUAGE("LG_rd_col_raw"));
    setHeaderText(3, LANGUAGE("LG_rd_col_eng"));
    setHeaderText(4, LANGUAGE("LG_rd_col_unit"));
    setHeaderText(5, LANGUAGE("LG_rd_col_rtt"));
    setHeaderText(6, LANGUAGE("LG_rd_col_status"));
    setHeaderText(7, LANGUAGE("LG_rd_col_updateTime"));

    ui->labelDbTip->setText(LANGUAGE("LG_rd_dbTip"));
    ui->labelTipDoubleClick->setText(LANGUAGE("LG_rd_doubleClickTip"));
}

// 通道列自适应剩余宽度，数值与状态列使用固定宽度
void RealDataWidget::initTable()
{
    QTableWidget *table = ui->tableWidgetRealData;
    QHeaderView *header = table->horizontalHeader();

    header->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < kColumnCount; ++column) {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
    }

    table->setColumnWidth(1, 80);   // 寄存器
    table->setColumnWidth(2, 70);   // 原始值
    table->setColumnWidth(3, 70);   // 工程值
    table->setColumnWidth(4, 56);   // 单位
    table->setColumnWidth(5, 60);   // RTT
    table->setColumnWidth(6, 76);   // 状态
    table->setColumnWidth(7, 84);   // 更新时间

    header->setFixedHeight(26);
    table->verticalHeader()->setDefaultSectionSize(32);
}

void RealDataWidget::setHeaderText(int column, const QString &text)
{
    QTableWidgetItem *item = ui->tableWidgetRealData->horizontalHeaderItem(column);
    if (item) {
        item->setText(text);
        return;
    }

    ui->tableWidgetRealData->setHorizontalHeaderItem(column, new QTableWidgetItem(text));
}
