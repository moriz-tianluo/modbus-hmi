#include "channelsetwidget.h"
#include "ui_channelsetwidget.h"
#include "Language.h"
#include "common/channelmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPixmap>
#include <QTableWidgetItem>

namespace {
// 通道色点颜色，与实时曲线的 4 条 graph 一一对应
const QColor kChannelColors[4] = {
    QColor(0x3B, 0x82, 0xF6),  // 蓝：温度
    QColor(0xF5, 0x9E, 0x0B),  // 橙：湿度
    QColor(0x10, 0xB9, 0x81),  // 绿：电压
    QColor(0x8B, 0x5C, 0xF6)   // 紫：电流
};
const int kChannelColorCount = 4;
}

ChannelSetWidget::ChannelSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ChannelSetWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    initTable();
    initConnections();
    languageChange();
}

ChannelSetWidget::~ChannelSetWidget()
{
    delete ui;
}

void ChannelSetWidget::setChannelModel(ChannelModel *model)
{
    m_channelModel = model;
    syncBufferCombo();
    rebuildTable();
}

void ChannelSetWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_channelConfig"));
    updateSelectAllButtonText();

    // 表头文字
    setHeaderText(0, LANGUAGE("LG_table_channel"));
    setHeaderText(1, LANGUAGE("LG_table_addr"));
    setHeaderText(2, LANGUAGE("LG_table_curValue"));

    ui->labelBuffer->setText(LANGUAGE("LG_channel_buffer"));
    ui->checkBoxSaveSqlite->setText(LANGUAGE("LG_channel_saveSqlite"));
    ui->pushButtonImportConfig->setText(LANGUAGE("LG_channel_importConfig"));
    ui->pushButtonExportJson->setText(LANGUAGE("LG_channel_exportJson"));
    ui->labelConfigPath->setText(LANGUAGE("LG_channel_configPath"));

    // 底部汇总跟随实际通道数/启用数
    refreshSummary();
}

void ChannelSetWidget::initConnections()
{
    connect(ui->pushButtonSelectAll, &QPushButton::clicked,
            this, &ChannelSetWidget::onSelectAllClicked);
    connect(ui->pushButtonImportConfig, &QPushButton::clicked,
            this, &ChannelSetWidget::onImportClicked);
    connect(ui->pushButtonExportJson, &QPushButton::clicked,
            this, &ChannelSetWidget::onExportClicked);

    connect(ui->tableWidgetChannel, &QTableWidget::itemChanged,
            this, &ChannelSetWidget::onChannelItemChanged);
    connect(ui->comboBoxBuffer, &QComboBox::currentIndexChanged,
            this, &ChannelSetWidget::onBufferChanged);
    connect(ui->checkBoxSaveSqlite, &QCheckBox::toggled,
            this, &ChannelSetWidget::onSaveToDatabaseToggled);
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

void ChannelSetWidget::rebuildTable()
{
    QTableWidget *table = ui->tableWidgetChannel;

    // 重建期间 itemChanged 会因 setCheckState/setForeground 反复触发，先屏蔽
    m_updating = true;
    table->setRowCount(0);

    if (m_channelModel == nullptr) {
        m_updating = false;
        refreshSummary();
        return;
    }

    const int count = m_channelModel->channelCount();
    table->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const ChannelItem &item = m_channelModel->channelAt(row);

        // 第 0 列：通道名称（勾选框 = 是否参与采集，色点 = 曲线颜色）
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.name);
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsUserCheckable);
        nameItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        nameItem->setIcon(channelColorIcon(row));
        table->setItem(row, 0, nameItem);

        // 第 1 列：寄存器地址（十六进制）
        table->setItem(row, 1,
            new QTableWidgetItem(QStringLiteral("0x%1").arg(item.registerAddress, 4, 16, QLatin1Char('0'))));

        // 第 2 列：当前原始值
        table->setItem(row, 2, new QTableWidgetItem(QString::number(item.rawValue)));

        applyRowStyle(row, item.enabled);
    }

    m_updating = false;
    refreshSummary();
}

void ChannelSetWidget::applyRowStyle(int row, bool enabled)
{
    QTableWidget *table = ui->tableWidgetChannel;
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

void ChannelSetWidget::refreshSummary()
{
    const int total = (m_channelModel != nullptr) ? m_channelModel->channelCount() : 0;
    const int enabled = (m_channelModel != nullptr) ? m_channelModel->enabledCount() : 0;
    ui->labelChannelSummary->setText(
        LANGUAGE("LG_channel_summaryFmt").arg(total).arg(enabled));

    // 启用状态变了，全选/全不选按钮文字要跟着切换
    updateSelectAllButtonText();
}

void ChannelSetWidget::updateSelectAllButtonText()
{
    const int total = (m_channelModel != nullptr) ? m_channelModel->channelCount() : 0;
    const int enabled = (m_channelModel != nullptr) ? m_channelModel->enabledCount() : 0;
    const bool allEnabled = (total > 0 && enabled == total);

    ui->pushButtonSelectAll->setText(
        allEnabled ? LANGUAGE("LG_channel_unselectAll") : LANGUAGE("LG_channel_selectAll"));
}

void ChannelSetWidget::syncBufferCombo()
{
    if (m_channelModel == nullptr) {
        return;
    }

    const int index = ui->comboBoxBuffer->findText(QString::number(m_channelModel->bufferSize()));
    if (index >= 0) {
        ui->comboBoxBuffer->setCurrentIndex(index);
    }
}

QIcon ChannelSetWidget::channelColorIcon(int index) const
{
    QColor color;
    // 优先使用通道自身配置的颜色，未配置时按索引取默认色
    if (m_channelModel != nullptr && index >= 0 && index < m_channelModel->channelCount()
        && m_channelModel->channelAt(index).color.isValid()) {
        color = m_channelModel->channelAt(index).color;
    } else {
        color = (index >= 0 && index < kChannelColorCount)
            ? kChannelColors[index]
            : QColor(0xC0, 0xC6, 0xD0);
    }

    QPixmap pixmap(10, 10);
    pixmap.fill(color);
    return QIcon(pixmap);
}

void ChannelSetWidget::onSelectAllClicked()
{
    if (m_channelModel == nullptr) {
        return;
    }

    // 全部已启用 → 本次点击执行全不选；只要有一个未启用 → 执行全选
    const bool allEnabled = (m_channelModel->channelCount() > 0
                             && m_channelModel->enabledCount() == m_channelModel->channelCount());
    setAllChannelsEnabled(!allEnabled);
}

void ChannelSetWidget::setAllChannelsEnabled(bool enabled)
{
    if (m_channelModel == nullptr) {
        return;
    }

    // 模型会发射 channelsChanged，由 onChannelsChanged 统一重建表格
    m_channelModel->setAllChannelsEnabled(enabled);
}

void ChannelSetWidget::onChannelItemChanged(QTableWidgetItem *item)
{
    if (m_updating || item == nullptr || item->column() != 0 || m_channelModel == nullptr) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= m_channelModel->channelCount()) {
        return;
    }

    const bool enabled = (item->checkState() == Qt::Checked);
    if (m_channelModel->channelAt(row).enabled == enabled) {
        return;
    }

    m_channelModel->setChannelEnabled(row, enabled);

    // 只刷新该行样式，避免整表重建打断用户操作
    m_updating = true;
    applyRowStyle(row, enabled);
    m_updating = false;
    refreshSummary();
}

void ChannelSetWidget::onBufferChanged(int index)
{
    Q_UNUSED(index);
    if (m_channelModel == nullptr) {
        return;
    }

    const int count = ui->comboBoxBuffer->currentText().toInt();
    if (count > 0) {
        m_channelModel->setBufferSize(count);
    }
}

void ChannelSetWidget::onSaveToDatabaseToggled(bool checked)
{
    m_saveToDatabase = checked;
    emit saveToDatabaseToggled(m_saveToDatabase);
}

bool ChannelSetWidget::saveToDatabase() const
{
    return m_saveToDatabase;
}

void ChannelSetWidget::setSaveToDatabase(bool enabled)
{
    // 勾选状态变化会触发 onSaveToDatabaseToggled，从而同步 m_saveToDatabase
    ui->checkBoxSaveSqlite->setChecked(enabled);
}

int ChannelSetWidget::bufferSize() const
{
    return ui->comboBoxBuffer->currentText().toInt();
}

void ChannelSetWidget::setBufferSizeValue(int size)
{
    if (size <= 0) {
        return;
    }

    int index = ui->comboBoxBuffer->findText(QString::number(size));
    if (index < 0) {
        ui->comboBoxBuffer->addItem(QString::number(size));
        index = ui->comboBoxBuffer->count() - 1;
    }
    // 切换下拉项会触发 onBufferChanged，把点数写回 ChannelModel
    ui->comboBoxBuffer->setCurrentIndex(index);
}

void ChannelSetWidget::onImportClicked()
{
    if (m_channelModel == nullptr) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this, LANGUAGE("LG_channel_importTitle"), QString(),
        LANGUAGE("LG_channel_importFilter"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_channelModel->loadFromJson(filePath, &errorMessage)) {
        QMessageBox::warning(this, LANGUAGE("LG_channel_msgTitle"), errorMessage);
        return;
    }

    ui->labelConfigPath->setText(filePath);
    emit configPathChanged(filePath);
    QMessageBox::information(this, LANGUAGE("LG_channel_msgTitle"),
        LANGUAGE("LG_channel_importDoneFmt").arg(m_channelModel->channelCount()));
}

void ChannelSetWidget::onExportClicked()
{
    if (m_channelModel == nullptr) {
        return;
    }

    const QString defaultPath = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config/channels.json"));
    const QString filePath = QFileDialog::getSaveFileName(
        this, LANGUAGE("LG_channel_exportTitle"), defaultPath,
        LANGUAGE("LG_channel_exportFilter"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_channelModel->saveToJson(filePath, &errorMessage)) {
        QMessageBox::warning(this, LANGUAGE("LG_channel_msgTitle"), errorMessage);
        return;
    }

    ui->labelConfigPath->setText(filePath);
    emit configPathChanged(filePath);
    QMessageBox::information(this, LANGUAGE("LG_channel_msgTitle"),
        LANGUAGE("LG_channel_exportDoneFmt").arg(filePath));
}

void ChannelSetWidget::onChannelsChanged()
{
    rebuildTable();
}

void ChannelSetWidget::onChannelValueChanged(int index, double value)
{
    Q_UNUSED(value);
    if (m_channelModel == nullptr || index < 0
        || index >= m_channelModel->channelCount()) {
        return;
    }

    // 只更新当前值列
    QTableWidgetItem *item = ui->tableWidgetChannel->item(index, 2);
    if (item != nullptr) {
        item->setText(QString::number(m_channelModel->channelAt(index).rawValue));
    }
}
