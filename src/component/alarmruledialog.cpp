#include "alarmruledialog.h"
#include "Language.h"
#include "common/channelmodel.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

AlarmRuleDialog::AlarmRuleDialog(ChannelModel *model, int debounceCount, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_debounceCount(qBound(1, debounceCount, 100))
{
    setWindowTitle(LANGUAGE("LG_alarm_ruleTitle"));
    resize(560, 460);

    buildUi();
    populateTable();
}

int AlarmRuleDialog::debounceCount() const
{
    return m_debounceCount;
}

// ============== 界面搭建 ==============

void AlarmRuleDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *tip = new QLabel(LANGUAGE("LG_alarm_ruleTip"), this);
    tip->setWordWrap(true);
    layout->addWidget(tip);

    // ---- 通道规则表格 ----
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        LANGUAGE("LG_alarm_ruleEnable"),
        LANGUAGE("LG_rd_col_channel"),
        LANGUAGE("LG_alarm_ruleHigh"),
        LANGUAGE("LG_alarm_ruleLow"),
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 70);
    m_table->setColumnWidth(2, 130);
    m_table->setColumnWidth(3, 130);
    layout->addWidget(m_table, 1);

    // ---- 连续越限次数 ----
    QHBoxLayout *debounceLayout = new QHBoxLayout;
    debounceLayout->addWidget(new QLabel(LANGUAGE("LG_alarm_ruleDebounce"), this));
    m_debounceSpin = new QSpinBox(this);
    m_debounceSpin->setRange(1, 100);
    m_debounceSpin->setValue(m_debounceCount);
    debounceLayout->addWidget(m_debounceSpin);
    debounceLayout->addStretch();
    layout->addLayout(debounceLayout);

    // ---- 确定 / 取消 ----
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(LANGUAGE("LG_channel_btnApply"));
    buttons->button(QDialogButtonBox::Cancel)->setText(LANGUAGE("LG_channel_btnClose"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AlarmRuleDialog::onAcceptClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void AlarmRuleDialog::populateTable()
{
    m_table->setRowCount(0);
    if (m_model == nullptr) {
        return;
    }

    const int count = m_model->channelCount();
    m_table->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const ChannelItem &item = m_model->channelAt(row);

        QTableWidgetItem *enableItem = new QTableWidgetItem;
        enableItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        enableItem->setCheckState(item.alarmEnabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 0, enableItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(
            item.unit.isEmpty() ? item.name
                                : QStringLiteral("%1 (%2)").arg(item.name, item.unit));
        nameItem->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(row, 1, nameItem);

        // 上下限用 QDoubleSpinBox 单元格，避免手工解析文本
        QDoubleSpinBox *highSpin = new QDoubleSpinBox(m_table);
        highSpin->setRange(-1000000000.0, 1000000000.0);
        highSpin->setDecimals(3);
        highSpin->setValue(item.alarmHigh);
        m_table->setCellWidget(row, 2, highSpin);

        QDoubleSpinBox *lowSpin = new QDoubleSpinBox(m_table);
        lowSpin->setRange(-1000000000.0, 1000000000.0);
        lowSpin->setDecimals(3);
        lowSpin->setValue(item.alarmLow);
        m_table->setCellWidget(row, 3, lowSpin);
    }
}

// ============== 规则写回 ==============

void AlarmRuleDialog::onAcceptClicked()
{
    if (!applyRules()) {
        return;
    }

    m_debounceCount = m_debounceSpin->value();
    emit debounceCountChanged(m_debounceCount);
    accept();
}

bool AlarmRuleDialog::applyRules()
{
    if (m_model == nullptr) {
        return false;
    }

    QVector<ChannelItem> channels = m_model->channels();
    for (int row = 0; row < channels.size() && row < m_table->rowCount(); ++row) {
        QTableWidgetItem *enableItem = m_table->item(row, 0);
        QDoubleSpinBox *highSpin = qobject_cast<QDoubleSpinBox *>(
            m_table->cellWidget(row, 2));
        QDoubleSpinBox *lowSpin = qobject_cast<QDoubleSpinBox *>(
            m_table->cellWidget(row, 3));
        if (enableItem == nullptr || highSpin == nullptr || lowSpin == nullptr) {
            continue;
        }

        channels[row].alarmEnabled = (enableItem->checkState() == Qt::Checked);
        channels[row].alarmHigh = highSpin->value();
        channels[row].alarmLow = lowSpin->value();
    }

    // 一次性替换，只触发一次 channelsChanged
    m_model->setChannels(channels);
    return true;
}
