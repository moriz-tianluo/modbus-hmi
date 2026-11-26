#include "channeleditdialog.h"
#include "Language.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ChannelEditDialog::ChannelEditDialog(ChannelModel *model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(LANGUAGE("LG_channel_mgrTitle"));
    resize(860, 560);

    buildUi();
    refreshTable();
    clearForm();
}

// ============== 界面搭建 ==============

void ChannelEditDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // ---- 通道列表 ----
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        LANGUAGE("LG_table_channel"),
        LANGUAGE("LG_channel_formSlave"),
        LANGUAGE("LG_channel_formFunction"),
        LANGUAGE("LG_channel_formAddr"),
        LANGUAGE("LG_channel_formType"),
        LANGUAGE("LG_channel_formRange"),
        LANGUAGE("LG_channel_formDecimals"),
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(2, 140);
    m_table->setColumnWidth(3, 110);
    m_table->setColumnWidth(4, 110);
    m_table->setColumnWidth(5, 140);
    m_table->setColumnWidth(6, 80);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ChannelEditDialog::onTableSelectionChanged);

    buildForm(layout);

    // ---- 底部按钮 ----
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *addButton = new QPushButton(LANGUAGE("LG_channel_btnAdd"), this);
    m_removeButton = new QPushButton(LANGUAGE("LG_channel_btnRemove"), this);
    m_applyButton = new QPushButton(LANGUAGE("LG_channel_btnApply"), this);
    QPushButton *closeButton = new QPushButton(LANGUAGE("LG_channel_btnClose"), this);

    connect(addButton, &QPushButton::clicked, this, &ChannelEditDialog::onAddClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &ChannelEditDialog::onRemoveClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &ChannelEditDialog::onApplyClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void ChannelEditDialog::buildForm(QVBoxLayout *layout)
{
    QGroupBox *group = new QGroupBox(LANGUAGE("LG_channel_formTitle"), this);
    QFormLayout *form = new QFormLayout(group);

    m_nameEdit = new QLineEdit(group);
    m_slaveSpin = new QSpinBox(group);
    m_slaveSpin->setRange(1, 247);

    m_functionCombo = new QComboBox(group);
    m_functionCombo->addItem(LANGUAGE("LG_channel_funcHolding"),
                             static_cast<int>(ModbusFunctionCode::ReadHoldingRegisters));
    m_functionCombo->addItem(LANGUAGE("LG_channel_funcInput"),
                             static_cast<int>(ModbusFunctionCode::ReadInputRegisters));

    m_addressSpin = new QSpinBox(group);
    m_addressSpin->setRange(0, 65535);
    m_addressSpin->setDisplayIntegerBase(10);

    m_typeCombo = new QComboBox(group);
    m_typeCombo->addItem(LANGUAGE("LG_channel_typeUnsigned"),
                         static_cast<int>(ChannelDataType::Unsigned16));
    m_typeCombo->addItem(LANGUAGE("LG_channel_typeSigned"),
                         static_cast<int>(ChannelDataType::Signed16));

    m_unitEdit = new QLineEdit(group);

    m_scaleSpin = new QDoubleSpinBox(group);
    m_scaleSpin->setRange(0.0001, 1000.0);
    m_scaleSpin->setDecimals(4);
    m_scaleSpin->setSingleStep(0.1);

    m_rangeMinSpin = new QDoubleSpinBox(group);
    m_rangeMaxSpin = new QDoubleSpinBox(group);
    QDoubleSpinBox *rangeSpins[] = { m_rangeMinSpin, m_rangeMaxSpin };
    for (QDoubleSpinBox *spin : rangeSpins) {
        spin->setRange(-1000000000.0, 1000000000.0);
        spin->setDecimals(3);
    }

    m_decimalsSpin = new QSpinBox(group);
    m_decimalsSpin->setRange(0, 6);

    m_enabledCheck = new QCheckBox(LANGUAGE("LG_channel_formEnabled"), group);
    m_enabledCheck->setChecked(true);

    m_colorButton = new QPushButton(group);
    connect(m_colorButton, &QPushButton::clicked,
            this, &ChannelEditDialog::onPickColorClicked);

    form->addRow(LANGUAGE("LG_channel_formName"), m_nameEdit);
    form->addRow(LANGUAGE("LG_channel_formSlave"), m_slaveSpin);
    form->addRow(LANGUAGE("LG_channel_formFunction"), m_functionCombo);
    form->addRow(LANGUAGE("LG_channel_formAddr"), m_addressSpin);
    form->addRow(LANGUAGE("LG_channel_formType"), m_typeCombo);
    form->addRow(LANGUAGE("LG_channel_formUnit"), m_unitEdit);
    form->addRow(LANGUAGE("LG_channel_formScale"), m_scaleSpin);
    form->addRow(LANGUAGE("LG_channel_formRangeMin"), m_rangeMinSpin);
    form->addRow(LANGUAGE("LG_channel_formRangeMax"), m_rangeMaxSpin);
    form->addRow(LANGUAGE("LG_channel_formDecimals"), m_decimalsSpin);
    form->addRow(LANGUAGE("LG_channel_formColor"), m_colorButton);
    form->addRow(QString(), m_enabledCheck);

    layout->addWidget(group);
}

// ============== 表格与表单同步 ==============

void ChannelEditDialog::refreshTable()
{
    QSignalBlocker blocker(m_table);
    m_table->setRowCount(0);

    if (m_model == nullptr) {
        return;
    }

    const int count = m_model->channelCount();
    m_table->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const ChannelItem &item = m_model->channelAt(row);

        QTableWidgetItem *nameItem = new QTableWidgetItem(item.name);
        nameItem->setData(Qt::DecorationRole, QColor(item.color.isValid()
                                                         ? item.color
                                                         : QColor(0xC0, 0xC6, 0xD0)));
        m_table->setItem(row, 0, nameItem);

        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(item.slaveId)));
        m_table->setItem(row, 2, new QTableWidgetItem(functionCodeText(item.functionCode)));
        m_table->setItem(row, 3, new QTableWidgetItem(
            QString::number(item.registerAddress)));
        m_table->setItem(row, 4, new QTableWidgetItem(dataTypeText(item.dataType)));
        m_table->setItem(row, 5, new QTableWidgetItem(
            QStringLiteral("%1 ~ %2").arg(item.rangeMin, 0, 'f', 2)
                                     .arg(item.rangeMax, 0, 'f', 2)));
        m_table->setItem(row, 6, new QTableWidgetItem(QString::number(item.decimals)));
    }

    if (m_currentIndex >= 0 && m_currentIndex < count) {
        m_table->selectRow(m_currentIndex);
    }
}

void ChannelEditDialog::loadChannelToForm(int index)
{
    if (m_model == nullptr || index < 0 || index >= m_model->channelCount()) {
        return;
    }

    const ChannelItem &item = m_model->channelAt(index);
    m_currentIndex = index;

    m_nameEdit->setText(item.name);
    m_slaveSpin->setValue(item.slaveId);
    m_functionCombo->setCurrentIndex(
        item.functionCode == ModbusFunctionCode::ReadInputRegisters ? 1 : 0);
    m_addressSpin->setValue(item.registerAddress);
    m_typeCombo->setCurrentIndex(item.dataType == ChannelDataType::Signed16 ? 1 : 0);
    m_unitEdit->setText(item.unit);
    m_scaleSpin->setValue(qFuzzyIsNull(item.scale) ? 1.0 : item.scale);
    m_rangeMinSpin->setValue(item.rangeMin);
    m_rangeMaxSpin->setValue(item.rangeMax);
    m_decimalsSpin->setValue(qBound(0, item.decimals, 6));
    m_enabledCheck->setChecked(item.enabled);

    m_color = item.color;
    updateColorButton();

    m_removeButton->setEnabled(true);
    m_applyButton->setText(LANGUAGE("LG_channel_btnApply"));
}

void ChannelEditDialog::clearForm()
{
    m_currentIndex = -1;

    m_nameEdit->clear();
    m_slaveSpin->setValue(1);
    m_functionCombo->setCurrentIndex(0);
    m_addressSpin->setValue(nextFreeAddress());
    m_typeCombo->setCurrentIndex(0);
    m_unitEdit->clear();
    m_scaleSpin->setValue(0.1);
    m_rangeMinSpin->setValue(0.0);
    m_rangeMaxSpin->setValue(100.0);
    m_decimalsSpin->setValue(2);
    m_enabledCheck->setChecked(true);

    m_color = QColor(0x3B, 0x82, 0xF6);
    updateColorButton();

    m_removeButton->setEnabled(false);
    m_applyButton->setText(LANGUAGE("LG_channel_btnAppend"));
}

// ============== 槽函数 ==============

void ChannelEditDialog::onTableSelectionChanged()
{
    const int row = m_table->currentRow();
    if (row >= 0) {
        loadChannelToForm(row);
    }
}

void ChannelEditDialog::onAddClicked()
{
    if (m_model == nullptr) {
        return;
    }

    // 新增先按默认值落库，随后自动选中该行供用户继续编辑
    ChannelItem item;
    item.name = LANGUAGE("LG_channel_newNameFmt").arg(m_model->channelCount() + 1);
    item.registerAddress = nextFreeAddress();
    item.dataType = ChannelDataType::Unsigned16;
    item.scale = 0.1;
    item.rangeMin = 0.0;
    item.rangeMax = 100.0;
    item.decimals = 2;
    item.color = m_color.isValid() ? m_color : QColor(0x3B, 0x82, 0xF6);
    item.enabled = true;
    item.alarmHigh = item.rangeMax;
    item.alarmLow = item.rangeMin;

    m_model->addChannel(item);
    m_currentIndex = m_model->channelCount() - 1;

    refreshTable();
    loadChannelToForm(m_currentIndex);
}

void ChannelEditDialog::onRemoveClicked()
{
    if (m_model == nullptr || m_currentIndex < 0
        || m_currentIndex >= m_model->channelCount()) {
        QMessageBox::warning(this, LANGUAGE("LG_channel_mgrTitle"),
                             LANGUAGE("LG_channel_errNoSelection"));
        return;
    }

    const QString name = m_model->channelAt(m_currentIndex).name;
    const auto answer = QMessageBox::question(
        this, LANGUAGE("LG_channel_removeTitle"),
        LANGUAGE("LG_channel_removeTextFmt").arg(name));
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_model->removeChannel(m_currentIndex);
    m_currentIndex = m_model->channelCount() > 0 ? 0 : -1;

    refreshTable();
    if (m_currentIndex >= 0) {
        loadChannelToForm(m_currentIndex);
    } else {
        clearForm();
    }
}

void ChannelEditDialog::onApplyClicked()
{
    if (m_model == nullptr) {
        return;
    }

    ChannelItem item;
    if (!readForm(&item, m_currentIndex)) {
        return;
    }

    if (m_currentIndex < 0) {
        m_model->addChannel(item);
        m_currentIndex = m_model->channelCount() - 1;
    } else {
        m_model->updateChannel(m_currentIndex, item);
    }

    refreshTable();
    loadChannelToForm(m_currentIndex);
}

void ChannelEditDialog::onPickColorClicked()
{
    const QColor color = QColorDialog::getColor(
        m_color.isValid() ? m_color : QColor(0x3B, 0x82, 0xF6),
        this, LANGUAGE("LG_channel_formColor"));
    if (!color.isValid()) {
        return;
    }

    m_color = color;
    updateColorButton();
}

// ============== 表单读写辅助 ==============

bool ChannelEditDialog::readForm(ChannelItem *item, int ignoreIndex)
{
    if (item == nullptr || m_model == nullptr) {
        return false;
    }

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, LANGUAGE("LG_channel_mgrTitle"),
                             LANGUAGE("LG_channel_errNameEmpty"));
        return false;
    }

    const quint8 slaveId = static_cast<quint8>(m_slaveSpin->value());
    const quint16 address = static_cast<quint16>(m_addressSpin->value());
    const ModbusFunctionCode functionCode =
        static_cast<ModbusFunctionCode>(m_functionCombo->currentData().toInt());
    for (int index = 0; index < m_model->channelCount(); ++index) {
        // 只拦同一从站同一功能码下的重复地址；03 与 04 可共用地址
        if (index != ignoreIndex
            && m_model->channelAt(index).slaveId == slaveId
            && m_model->channelAt(index).functionCode == functionCode
            && m_model->channelAt(index).registerAddress == address) {
            QMessageBox::warning(this, LANGUAGE("LG_channel_mgrTitle"),
                                 LANGUAGE("LG_channel_errDuplicateAddr"));
            return false;
        }
    }

    if (m_rangeMaxSpin->value() <= m_rangeMinSpin->value()) {
        QMessageBox::warning(this, LANGUAGE("LG_channel_mgrTitle"),
                             LANGUAGE("LG_channel_errInvalidRange"));
        return false;
    }

    // 编辑时保留原有报警规则，避免改名称/单位后阈值被重置
    ChannelItem result;
    if (ignoreIndex >= 0 && ignoreIndex < m_model->channelCount()) {
        result = m_model->channelAt(ignoreIndex);
    }

    result.name = name;
    result.slaveId = slaveId;
    result.functionCode = static_cast<ModbusFunctionCode>(m_functionCombo->currentData().toInt());
    result.registerAddress = address;
    result.dataType = static_cast<ChannelDataType>(m_typeCombo->currentData().toInt());
    result.unit = m_unitEdit->text().trimmed();
    result.scale = m_scaleSpin->value();
    result.rangeMin = m_rangeMinSpin->value();
    result.rangeMax = m_rangeMaxSpin->value();
    result.decimals = m_decimalsSpin->value();
    result.color = m_color;
    result.enabled = m_enabledCheck->isChecked();

    if (ignoreIndex < 0) {
        // 新增通道的报警阈值默认取量程边界
        result.alarmHigh = result.rangeMax;
        result.alarmLow = result.rangeMin;
    }

    *item = result;
    return true;
}

void ChannelEditDialog::updateColorButton()
{
    if (m_colorButton == nullptr) {
        return;
    }

    const QColor color = m_color.isValid() ? m_color : QColor(0xC0, 0xC6, 0xD0);
    // 浅色背景配深色文字，避免按钮文字看不清
    const QString textColor = color.lightness() > 160
        ? QStringLiteral("#1F2937")
        : QStringLiteral("#FFFFFF");
    m_colorButton->setText(LANGUAGE("LG_channel_btnColor"));
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2; border: 1px solid #C9D2E0;"
                       " border-radius: 4px; padding: 4px 10px;")
            .arg(color.name(), textColor));
}

QString ChannelEditDialog::dataTypeText(ChannelDataType type) const
{
    return type == ChannelDataType::Signed16
        ? LANGUAGE("LG_channel_typeSigned")
        : LANGUAGE("LG_channel_typeUnsigned");
}

QString ChannelEditDialog::functionCodeText(ModbusFunctionCode code) const
{
    return code == ModbusFunctionCode::ReadInputRegisters
        ? LANGUAGE("LG_channel_funcInput")
        : LANGUAGE("LG_channel_funcHolding");
}

quint16 ChannelEditDialog::nextFreeAddress() const
{
    if (m_model == nullptr || m_model->channelCount() == 0) {
        return 0;
    }

    quint16 maxAddress = 0;
    for (int index = 0; index < m_model->channelCount(); ++index) {
        maxAddress = qMax(maxAddress, m_model->channelAt(index).registerAddress);
    }
    return static_cast<quint16>(maxAddress + 1);
}
