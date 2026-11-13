#include "alarmandfaultwidget.h"
#include "ui_alarmandfaultwidget.h"
#include "Language.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

AlarmAndFaultWidget::AlarmAndFaultWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::AlarmAndFaultWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);

    // 缓存三行控件指针，便于按记录循环刷新
    m_itemWidgets[0] = ui->widgetAlarmItem1;
    m_itemWidgets[1] = ui->widgetAlarmItem2;
    m_itemWidgets[2] = ui->widgetAlarmItem3;
    m_timeLabels[0] = ui->labelAlarmTime1;
    m_timeLabels[1] = ui->labelAlarmTime2;
    m_timeLabels[2] = ui->labelAlarmTime3;
    m_tagLabels[0] = ui->labelAlarmTag1;
    m_tagLabels[1] = ui->labelAlarmTag2;
    m_tagLabels[2] = ui->labelAlarmTag3;
    m_msgLabels[0] = ui->labelAlarmMsg1;
    m_msgLabels[1] = ui->labelAlarmMsg2;
    m_msgLabels[2] = ui->labelAlarmMsg3;

    connect(ui->pushButtonClear, &QPushButton::clicked,
            this, &AlarmAndFaultWidget::clearAlarms);
    connect(ui->pushButtonViewAll, &QPushButton::clicked,
            this, &AlarmAndFaultWidget::showAllRecords);

    languageChange();
    refreshList();
}

AlarmAndFaultWidget::~AlarmAndFaultWidget()
{
    delete ui;
}

void AlarmAndFaultWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_alarmFault"));

    // 底部说明与按钮（条目标签/描述由 refreshList 按真实记录填充）
    ui->labelFilterTip->setText(LANGUAGE("LG_alarm_filterTip"));
    ui->labelParamTip->setText(LANGUAGE("LG_alarm_paramTip"));
    ui->pushButtonViewAll->setText(LANGUAGE("LG_alarm_btn_all"));
    ui->pushButtonClear->setText(LANGUAGE("LG_alarm_btn_clear"));

    // 语言切换后条目标签与总数需重新生成
    refreshList();
}

void AlarmAndFaultWidget::setParamTip(const QString &text)
{
    ui->labelParamTip->setText(text);
}

void AlarmAndFaultWidget::addAlarm(AlarmLevel level, const QString &message)
{
    const QDateTime now = QDateTime::currentDateTime();

    // 去抖：与最新记录等级/内容相同且间隔很短时，仅刷新时间
    if (!m_records.isEmpty()) {
        AlarmRecord &latest = m_records.first();
        if (latest.level == level && latest.message == message
            && latest.time.msecsTo(now) < kDebounceMs) {
            latest.time = now;
            refreshList();
            return;
        }
    }

    AlarmRecord record;
    record.time = now;
    record.level = level;
    record.message = message;
    m_records.prepend(record);

    // 去抖后确认是新记录，通知数据库等旁路
    emit alarmRecordAdded(static_cast<int>(level), message);

    // 超出容量上限时丢弃最旧记录
    while (m_records.size() > kMaxRecords) {
        m_records.removeLast();
    }

    refreshList();
}

void AlarmAndFaultWidget::clearAlarms()
{
    m_records.clear();
    refreshList();
}

void AlarmAndFaultWidget::refreshList()
{
    for (int i = 0; i < kDisplayRows; ++i) {
        if (i < m_records.size()) {
            updateRow(i, m_records.at(i));
            m_itemWidgets[i]->show();
        } else {
            m_itemWidgets[i]->hide();
        }
    }

    // 角标与总数
    const int count = m_records.size();
    ui->labelAlarmCountBadge->setText(QString::number(count));
    ui->labelAlarmCountBadge->setVisible(count > 0);
    ui->labelTotalCount->setText(LANGUAGE("LG_alarm_totalCountFmt").arg(count));
}

void AlarmAndFaultWidget::updateRow(int rowIndex, const AlarmRecord &record)
{
    if (rowIndex < 0 || rowIndex >= kDisplayRows) {
        return;
    }

    m_timeLabels[rowIndex]->setText(record.time.toString(QStringLiteral("HH:mm:ss")));
    m_msgLabels[rowIndex]->setText(record.message);

    const bool isAlarm = (record.level == AlarmLevel::Alarm);
    m_tagLabels[rowIndex]->setText(
        isAlarm ? LANGUAGE("LG_alarm_tag_alarm") : LANGUAGE("LG_alarm_tag_fault"));
    applyTagStyle(m_tagLabels[rowIndex], record.level);
}

void AlarmAndFaultWidget::applyTagStyle(QLabel *tagLabel, AlarmLevel level)
{
    tagLabel->setProperty("role", level == AlarmLevel::Alarm
                                      ? QStringLiteral("tagAlarm")
                                      : QStringLiteral("tagFault"));
    // 动态属性变化后需要重新应用一次样式表
    tagLabel->style()->unpolish(tagLabel);
    tagLabel->style()->polish(tagLabel);
}

void AlarmAndFaultWidget::showAllRecords()
{
    QDialog dialog(this);
    dialog.setWindowTitle(LANGUAGE("LG_alarm_dialogTitle"));
    dialog.resize(560, 380);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTableWidget *table = new QTableWidget(m_records.size(), 3, &dialog);
    table->setHorizontalHeaderLabels({
        LANGUAGE("LG_alarm_col_time"),
        LANGUAGE("LG_alarm_col_type"),
        LANGUAGE("LG_alarm_col_message"),
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);

    for (int row = 0; row < m_records.size(); ++row) {
        const AlarmRecord &record = m_records.at(row);
        table->setItem(row, 0, new QTableWidgetItem(
            record.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        table->setItem(row, 1, new QTableWidgetItem(
            record.level == AlarmLevel::Alarm ? LANGUAGE("LG_alarm_tag_alarm")
                                              : LANGUAGE("LG_alarm_tag_fault")));
        table->setItem(row, 2, new QTableWidgetItem(record.message));
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(LANGUAGE("LG_alarm_btn_close"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(table);
    layout->addWidget(buttons);
    dialog.exec();
}
