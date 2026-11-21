#include "alarmhistorydialog.h"
#include "Language.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int kAlarmQueryLimit = 5000;
// 时间范围下拉框索引 → 小时数
constexpr int kRangeHours[] = {1, 6, 24, 24 * 7};
}

AlarmHistoryDialog::AlarmHistoryDialog(SaveDataSQL *saveData, QWidget *parent)
    : QDialog(parent)
    , m_saveData(saveData)
{
    initUi();
    initConnections();

    if (m_saveData != nullptr) {
        connect(m_saveData, &SaveDataSQL::alarmsQueried,
                this, &AlarmHistoryDialog::onAlarmsQueried);
        connect(m_saveData, &SaveDataSQL::alarmsAcknowledged,
                this, &AlarmHistoryDialog::onAlarmsAcknowledged);
        connect(m_saveData, &SaveDataSQL::alarmExportFinished,
                this, &AlarmHistoryDialog::onAlarmExportFinished);
    }
    refresh();
}

AlarmHistoryDialog::~AlarmHistoryDialog() = default;

void AlarmHistoryDialog::initUi()
{
    setWindowTitle(LANGUAGE("LG_alarm_history_title"));
    resize(820, 480);

    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(LANGUAGE("LG_store_range_1h"));
    m_rangeCombo->addItem(LANGUAGE("LG_store_range_6h"));
    m_rangeCombo->addItem(LANGUAGE("LG_store_range_24h"));
    m_rangeCombo->addItem(LANGUAGE("LG_store_range_7d"));
    m_rangeCombo->setCurrentIndex(2);

    m_refreshButton = new QPushButton(LANGUAGE("LG_alarm_history_refresh"), this);
    m_statsLabel = new QLabel(this);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(m_rangeCombo);
    topLayout->addWidget(m_refreshButton);
    topLayout->addWidget(m_statsLabel, 1);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        LANGUAGE("LG_alarm_history_colTime"),
        LANGUAGE("LG_alarm_history_colLevel"),
        LANGUAGE("LG_alarm_history_colMessage"),
        LANGUAGE("LG_alarm_history_colState"),
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 170);
    m_table->setColumnWidth(1, 70);
    m_table->setColumnWidth(3, 80);

    m_ackSelectedButton = new QPushButton(LANGUAGE("LG_alarm_history_ackSelected"), this);
    m_ackAllButton = new QPushButton(LANGUAGE("LG_alarm_history_ackAll"), this);
    m_exportButton = new QPushButton(LANGUAGE("LG_alarm_history_export"), this);
    m_closeButton = new QPushButton(LANGUAGE("LG_alarm_history_close"), this);

    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(m_ackSelectedButton);
    bottomLayout->addWidget(m_ackAllButton);
    bottomLayout->addWidget(m_exportButton);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topLayout);
    layout->addWidget(m_table, 1);
    layout->addLayout(bottomLayout);
}

void AlarmHistoryDialog::initConnections()
{
    connect(m_rangeCombo, &QComboBox::currentIndexChanged,
            this, &AlarmHistoryDialog::onRangeChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &AlarmHistoryDialog::onRefreshClicked);
    connect(m_ackSelectedButton, &QPushButton::clicked,
            this, &AlarmHistoryDialog::onAcknowledgeSelected);
    connect(m_ackAllButton, &QPushButton::clicked,
            this, &AlarmHistoryDialog::onAcknowledgeAll);
    connect(m_exportButton, &QPushButton::clicked, this, &AlarmHistoryDialog::onExport);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void AlarmHistoryDialog::onRangeChanged(int index)
{
    Q_UNUSED(index)
    refresh();
}

void AlarmHistoryDialog::onRefreshClicked()
{
    refresh();
}

void AlarmHistoryDialog::refresh()
{
    if (m_saveData == nullptr) {
        return;
    }

    const int index = qBound(0, m_rangeCombo->currentIndex(), 3);
    m_lastToMs = QDateTime::currentMSecsSinceEpoch();
    m_lastFromMs = m_lastToMs - static_cast<qint64>(kRangeHours[index]) * 3600000LL;

    QMetaObject::invokeMethod(m_saveData, "queryAlarms", Qt::QueuedConnection,
                              Q_ARG(qint64, m_lastFromMs),
                              Q_ARG(qint64, m_lastToMs),
                              Q_ARG(int, kAlarmQueryLimit));
}

void AlarmHistoryDialog::onAlarmsQueried(qint64 fromMs, qint64 toMs,
                                         const QVector<AlarmDbRecord> &rows)
{
    // 只处理当前时间范围的应答，避免连续切换范围时旧结果覆盖新结果
    if (fromMs != m_lastFromMs || toMs != m_lastToMs) {
        return;
    }

    m_rows = rows;
    m_table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const AlarmDbRecord &record = rows.at(row);
        auto *timeItem = new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(record.ts)
                .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        timeItem->setData(Qt::UserRole, record.id);
        m_table->setItem(row, 0, timeItem);
        m_table->setItem(row, 1, new QTableWidgetItem(levelText(record.level)));
        m_table->setItem(row, 2, new QTableWidgetItem(record.message));
        m_table->setItem(row, 3, new QTableWidgetItem(stateText(record.acknowledged)));
    }
    updateStatistics(rows);
}

void AlarmHistoryDialog::updateStatistics(const QVector<AlarmDbRecord> &rows)
{
    int alarmCount = 0;
    int faultCount = 0;
    int pendingCount = 0;
    for (const AlarmDbRecord &record : rows) {
        if (record.level == 1) {
            ++faultCount;
        } else {
            ++alarmCount;
        }
        if (!record.acknowledged) {
            ++pendingCount;
        }
    }

    m_statsLabel->setText(LANGUAGE("LG_alarm_statsFmt")
                              .arg(rows.size())
                              .arg(alarmCount)
                              .arg(faultCount)
                              .arg(pendingCount));
}

qint64 AlarmHistoryDialog::selectedAlarmId() const
{
    const int row = m_table->currentRow();
    if (row < 0) {
        return -1;
    }
    const QTableWidgetItem *item = m_table->item(row, 0);
    return item == nullptr ? -1 : item->data(Qt::UserRole).toLongLong();
}

void AlarmHistoryDialog::onAcknowledgeSelected()
{
    const qint64 id = selectedAlarmId();
    if (id <= 0 || m_saveData == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(m_saveData, "acknowledgeAlarm", Qt::QueuedConnection,
                              Q_ARG(qint64, id));
}

void AlarmHistoryDialog::onAcknowledgeAll()
{
    if (m_saveData == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(m_saveData, "acknowledgeAlarm", Qt::QueuedConnection,
                              Q_ARG(qint64, static_cast<qint64>(0)));
}

void AlarmHistoryDialog::onAlarmsAcknowledged(qint64 rows)
{
    m_statsLabel->setText(LANGUAGE("LG_alarm_ackDoneFmt").arg(rows));
    if (rows > 0) {
        refresh();
    }
}

void AlarmHistoryDialog::onExport()
{
    if (m_saveData == nullptr) {
        return;
    }

    const QString defaultName = QStringLiteral("alarm_%1.csv")
                                    .arg(QDateTime::currentDateTime()
                                             .toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, LANGUAGE("LG_alarm_exportTitle"), defaultName,
        LANGUAGE("LG_store_exportFilter"));
    if (path.isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(m_saveData, "exportAlarmsToCsv", Qt::QueuedConnection,
                              Q_ARG(qint64, m_lastFromMs),
                              Q_ARG(qint64, m_lastToMs),
                              Q_ARG(QString, path));
}

void AlarmHistoryDialog::onAlarmExportFinished(bool ok, qint64 rows,
                                               const QString &filePath,
                                               const QString &errorString)
{
    if (ok) {
        QMessageBox::information(this, LANGUAGE("LG_alarm_exportTitle"),
                                 LANGUAGE("LG_alarm_exportDoneFmt")
                                     .arg(rows)
                                     .arg(filePath));
        return;
    }
    const QString detail = errorString.isEmpty()
                               ? LANGUAGE("LG_store_err_dbNotReady")
                               : errorString;
    QMessageBox::warning(this, LANGUAGE("LG_alarm_exportTitle"),
                         LANGUAGE("LG_alarm_exportFailFmt").arg(detail));
}

QString AlarmHistoryDialog::levelText(int level)
{
    return level == 1 ? LANGUAGE("LG_alarm_level_fault") : LANGUAGE("LG_alarm_level_alarm");
}

QString AlarmHistoryDialog::stateText(bool acknowledged)
{
    return acknowledged ? LANGUAGE("LG_alarm_state_acked") : LANGUAGE("LG_alarm_state_pending");
}
