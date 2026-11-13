#include "realdatawidget.h"
#include "ui_realdatawidget.h"
#include "Language.h"
#include "common/channelmodel.h"

#include <QBrush>
#include <QDateTime>
#include <QHeaderView>
#include <QTableWidgetItem>

RealDataWidget::RealDataWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::RealDataWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    initTable();
    connect(ui->tableWidgetRealData, &QTableWidget::cellClicked,
            this, &RealDataWidget::onCellClicked);
    connect(ui->tableWidgetRealData, &QTableWidget::cellDoubleClicked,
            this, &RealDataWidget::onCellDoubleClicked);
    languageChange();
}

RealDataWidget::~RealDataWidget()
{
    delete ui;
}

void RealDataWidget::setChannelModel(ChannelModel *model)
{
    m_channelModel = model;
    rebuildTable();
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

    refreshDbTip();
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

void RealDataWidget::rebuildTable()
{
    QTableWidget *table = ui->tableWidgetRealData;
    table->setRowCount(0);
    m_highlightRow = -1;

    if (m_channelModel == nullptr) {
        return;
    }

    const int count = m_channelModel->channelCount();
    table->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const ChannelItem &item = m_channelModel->channelAt(row);

        table->setItem(row, 0, new QTableWidgetItem(item.name));
        table->setItem(row, 1,
            new QTableWidgetItem(QStringLiteral("0x%1").arg(item.registerAddress, 4, 16, QLatin1Char('0'))));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(item.rawValue)));
        table->setItem(row, 3, new QTableWidgetItem(
            QString::number(item.value(), 'f', qBound(0, item.decimals, 6))));
        table->setItem(row, 4, new QTableWidgetItem(item.unit));
        table->setItem(row, 5, new QTableWidgetItem(QStringLiteral("--")));
        table->setItem(row, 6, new QTableWidgetItem(
            item.enabled ? LANGUAGE("LG_rd_statusWaiting") : LANGUAGE("LG_rd_statusDisabled")));
        table->setItem(row, 7, new QTableWidgetItem(QStringLiteral("--")));

        // 未启用通道整行置灰，和通道配置卡片保持一致
        if (!item.enabled) {
            applyRowEnabledStyle(row, false);
        }
    }
}

void RealDataWidget::onChannelsChanged()
{
    // 通道名称/单位/地址/数量变化：整表重建，保持与通道配置一致
    rebuildTable();
}

void RealDataWidget::onChannelEnabledChanged(int index, bool enabled)
{
    if (m_channelModel == nullptr || index < 0
        || index >= m_channelModel->channelCount()) {
        return;
    }

    applyRowEnabledStyle(index, enabled);

    // 启用状态变化后状态列也要跟着切换
    QTableWidgetItem *statusItem = ui->tableWidgetRealData->item(index, 6);
    if (statusItem != nullptr && !enabled) {
        statusItem->setText(LANGUAGE("LG_rd_statusDisabled"));
    } else if (statusItem != nullptr) {
        statusItem->setText(LANGUAGE("LG_rd_statusWaiting"));
    }

    // 停用后不再参与采集，RTT 清空
    if (!enabled) {
        QTableWidgetItem *rttItem = ui->tableWidgetRealData->item(index, 5);
        if (rttItem != nullptr) {
            rttItem->setText(QStringLiteral("--"));
        }
    }
}

void RealDataWidget::applyRowEnabledStyle(int row, bool enabled)
{
    QTableWidget *table = ui->tableWidgetRealData;
    if (row < 0 || row >= table->rowCount()) {
        return;
    }

    for (int column = 0; column < table->columnCount(); ++column) {
        QTableWidgetItem *item = table->item(row, column);
        if (item == nullptr) {
            continue;
        }
        // 启用行用默认前景色，未启用行整行变灰
        item->setForeground(enabled ? QBrush() : QBrush(QColor(0xB0, 0xB8, 0xC4)));
    }
}

void RealDataWidget::selectChannelRow(int index)
{
    QTableWidget *table = ui->tableWidgetRealData;
    if (index < 0 || index >= table->rowCount()) {
        return;
    }

    // 再次点击同一通道（含图例重复点击）→ 取消高亮
    if (m_highlightRow == index) {
        clearChannelHighlight();
        return;
    }

    m_highlightRow = index;
    table->setCurrentCell(index, 0);
    emit channelSelected(index);
}

void RealDataWidget::setRoundTripTime(int milliseconds)
{
    QTableWidget *table = ui->tableWidgetRealData;
    const QString text = LANGUAGE("LG_rd_rttFmt").arg(milliseconds);

    for (int row = 0; row < table->rowCount(); ++row) {
        // 未启用通道不参与本轮采集，RTT 保持 --
        if (m_channelModel != nullptr && row < m_channelModel->channelCount()
            && !m_channelModel->channelAt(row).enabled) {
            continue;
        }

        QTableWidgetItem *rttItem = table->item(row, 5);
        if (rttItem != nullptr) {
            rttItem->setText(text);
        }
    }
}

void RealDataWidget::onCellClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= ui->tableWidgetRealData->rowCount()) {
        return;
    }

    // 同一行再次单击视为取消高亮
    if (m_highlightRow == row) {
        clearChannelHighlight();
        return;
    }

    m_highlightRow = row;
    emit channelSelected(row);
}

void RealDataWidget::onCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= ui->tableWidgetRealData->rowCount()) {
        return;
    }

    // 双击行 → 请求 MainWidget 打开该通道的历史查询
    emit historyRequested(row);
}

void RealDataWidget::clearChannelHighlight()
{
    if (m_highlightRow < 0) {
        return;
    }

    m_highlightRow = -1;
    ui->tableWidgetRealData->clearSelection();
    emit channelSelected(-1);
}

void RealDataWidget::onChannelValueChanged(int index, double value)
{
    Q_UNUSED(value);
    if (m_channelModel == nullptr || index < 0
        || index >= m_channelModel->channelCount()) {
        return;
    }

    const ChannelItem &item = m_channelModel->channelAt(index);
    QTableWidget *table = ui->tableWidgetRealData;
    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));

    // 更新原始值 / 工程值 / 状态 / 更新时间
    QTableWidgetItem *rawItem = table->item(index, 2);
    if (rawItem) rawItem->setText(QString::number(item.rawValue));

    QTableWidgetItem *engItem = table->item(index, 3);
    if (engItem) engItem->setText(
        QString::number(item.value(), 'f', qBound(0, item.decimals, 6)));

    QTableWidgetItem *statusItem = table->item(index, 6);
    if (statusItem) statusItem->setText(LANGUAGE("LG_rd_statusOk"));

    QTableWidgetItem *timeItem = table->item(index, 7);
    if (timeItem) timeItem->setText(timeStr);

    // 越限时把状态列改为胶囊文字并整行标红
    updateAlarmStyle(index);
}

void RealDataWidget::updateAlarmStyle(int index)
{
    QTableWidget *table = ui->tableWidgetRealData;
    if (m_channelModel == nullptr || index < 0
        || index >= m_channelModel->channelCount() || index >= table->rowCount()) {
        return;
    }

    const ChannelItem &item = m_channelModel->channelAt(index);
    // 停用通道保持置灰样式，不做越限判定
    if (!item.enabled) {
        return;
    }

    const double value = item.value();
    const bool overHigh = item.alarmEnabled && item.alarmHigh > item.alarmLow
                          && value > item.alarmHigh;
    const bool belowLow = item.alarmEnabled && item.alarmHigh > item.alarmLow
                          && value < item.alarmLow;

    QTableWidgetItem *statusItem = table->item(index, 6);
    if (statusItem != nullptr) {
        if (overHigh) {
            statusItem->setText(LANGUAGE("LG_status_overUpper"));
        } else if (belowLow) {
            statusItem->setText(LANGUAGE("LG_status_belowLower"));
        }
    }

    // 越限行整行红底红字，恢复后清除背景
    const bool alarming = overHigh || belowLow;
    for (int column = 0; column < table->columnCount(); ++column) {
        QTableWidgetItem *rowItem = table->item(index, column);
        if (rowItem == nullptr) {
            continue;
        }
        rowItem->setBackground(alarming ? QBrush(QColor(0xFF, 0xF1, 0xF0)) : QBrush());
        rowItem->setForeground(alarming ? QBrush(QColor(0xE0, 0x48, 0x3F)) : QBrush());
    }
}

void RealDataWidget::onRecordCountReady(qint64 fromMs, qint64 toMs, qint64 count)
{
    Q_UNUSED(toMs)

    // 只接受全量统计请求（fromMs == 0），避免卡片局部查询覆盖脚注
    if (fromMs != 0) {
        return;
    }

    m_totalRecords = count;
    refreshDbTip();
}

void RealDataWidget::refreshDbTip()
{
    ui->labelDbTip->setText(LANGUAGE("LG_rd_dbTipFmt").arg(m_totalRecords));
}
