#ifndef ALARMHISTORYDIALOG_H
#define ALARMHISTORYDIALOG_H

#include "savedatasql.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

// 报警历史对话框：按时间范围查询 alarm 表，支持确认、统计与 CSV 导出。
// 所有数据库操作通过 queued invokeMethod 投递到数据库线程执行。
class AlarmHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlarmHistoryDialog(SaveDataSQL *saveData, QWidget *parent = nullptr);
    ~AlarmHistoryDialog() override;

private:
    void initUi();
    void initConnections();
    // 按当前时间范围重新查询
    void refresh();
    // 当前选中行的报警 id，未选中返回 -1
    qint64 selectedAlarmId() const;
    void onAcknowledgeSelected();
    void onAcknowledgeAll();
    void onExport();
    // 数据库线程回调
    void onAlarmsQueried(qint64 fromMs, qint64 toMs, const QVector<AlarmDbRecord> &rows);
    void onAlarmsAcknowledged(qint64 rows);
    void onAlarmExportFinished(bool ok, qint64 rows, const QString &filePath,
                               const QString &errorString);
    // 刷新统计文本
    void updateStatistics(const QVector<AlarmDbRecord> &rows);
    static QString levelText(int level);
    static QString stateText(bool acknowledged);

private slots:
    void onRangeChanged(int index);
    void onRefreshClicked();

private:
    SaveDataSQL *m_saveData = nullptr;

    QComboBox *m_rangeCombo = nullptr;
    QLabel *m_statsLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_ackSelectedButton = nullptr;
    QPushButton *m_ackAllButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QVector<AlarmDbRecord> m_rows;
    qint64 m_lastFromMs = 0;
    qint64 m_lastToMs = 0;
};

#endif // ALARMHISTORYDIALOG_H
