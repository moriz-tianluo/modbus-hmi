#include <QtTest>

#include <QDateTime>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "savedatasql.h"

// SaveDataSQL 单元测试：批量落库、记录数统计、限行查询、CSV 字段掩码、过期清理。
// 所有方法在同一线程直接调用，信号以直连方式同步返回。
class TestSaveDataSql : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void commitAndCountRecords();
    void queryRespectsLimit();
    void exportUsesFieldMask();
    void purgeRemovesOldRows();
    void alarmQueryAcknowledgeExport();
    void backupAndVacuum();
    void restoreOverwritesCurrent();

private:
    static QVector<SampleChannelData> makeRound(qint64 timestampMs, int channelCount);
    void openDatabase();

    QScopedPointer<QTemporaryDir> m_tempDir;
    SaveDataSQL *m_database = nullptr;
};

void TestSaveDataSql::init()
{
    m_tempDir.reset(new QTemporaryDir());
    QVERIFY(m_tempDir->isValid());

    m_database = new SaveDataSQL();
    openDatabase();
}

void TestSaveDataSql::cleanup()
{
    delete m_database;
    m_database = nullptr;
    m_tempDir.reset();
}

QVector<SampleChannelData> TestSaveDataSql::makeRound(qint64 timestampMs, int channelCount)
{
    QVector<SampleChannelData> rows;
    for (int i = 0; i < channelCount; ++i) {
        SampleChannelData row;
        row.ts = timestampMs;
        row.channel = i;
        row.raw = static_cast<quint16>(100 + i);
        row.value = (100 + i) * 0.1;
        rows.append(row);
    }
    return rows;
}

void TestSaveDataSql::openDatabase()
{
    QSignalSpy spy(m_database, &SaveDataSQL::databaseOpened);
    m_database->openDatabase(m_tempDir->filePath(QStringLiteral("test.db")));

    if (spy.isEmpty()) {
        QVERIFY(spy.wait(3000));
    }
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
}

void TestSaveDataSql::commitAndCountRecords()
{
    m_database->enqueueSample(1000, makeRound(1000, 2));
    m_database->flush();

    QSignalSpy spy(m_database, &SaveDataSQL::recordCountReady);
    m_database->queryRecordCount(0, 5000);

    if (spy.isEmpty()) {
        QVERIFY(spy.wait(3000));
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(2).toLongLong(), static_cast<qint64>(2));
}

void TestSaveDataSql::queryRespectsLimit()
{
    m_database->enqueueSample(1000, makeRound(1000, 3));
    m_database->flush();

    QSignalSpy spy(m_database, &SaveDataSQL::samplesQueried);
    m_database->querySamples(0, 5000, -1, 2);

    if (spy.isEmpty()) {
        QVERIFY(spy.wait(3000));
    }
    QCOMPARE(spy.count(), 1);

    const QVector<SampleChannelData> rows =
        spy.at(0).at(2).value<QVector<SampleChannelData>>();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).ts, static_cast<qint64>(1000));
}

void TestSaveDataSql::exportUsesFieldMask()
{
    m_database->enqueueSample(1000, makeRound(1000, 1));
    m_database->flush();

    const QString csvPath = m_tempDir->filePath(QStringLiteral("export.csv"));
    const int fieldMask = SaveDataSQL::ExportTime | SaveDataSQL::ExportValue;

    QSignalSpy spy(m_database, &SaveDataSQL::exportFinished);
    m_database->exportSamplesToCsv(0, 5000, csvPath, fieldMask);

    if (spy.isEmpty()) {
        QVERIFY(spy.wait(3000));
    }
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
    QCOMPARE(spy.at(0).at(1).toLongLong(), static_cast<qint64>(1));

    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // 只导出时间（ts + 可读时间两列）与工程值
    QVERIFY(content.contains(QStringLiteral("ts,datetime,value")));
    QVERIFY(!content.contains(QStringLiteral("channel")));
    QVERIFY(!content.contains(QStringLiteral("raw")));
}

void TestSaveDataSql::purgeRemovesOldRows()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 oldMs = nowMs - 3LL * 86400000LL;  // 3 天前

    m_database->enqueueSample(oldMs, makeRound(oldMs, 2));
    m_database->enqueueSample(nowMs, makeRound(nowMs, 2));
    m_database->flush();

    QSignalSpy purgeSpy(m_database, &SaveDataSQL::purgedOlderThan);
    m_database->purgeOlderThan(1);

    if (purgeSpy.isEmpty()) {
        QVERIFY(purgeSpy.wait(3000));
    }
    QCOMPARE(purgeSpy.count(), 1);
    QCOMPARE(purgeSpy.at(0).at(1).toLongLong(), static_cast<qint64>(2));

    QSignalSpy countSpy(m_database, &SaveDataSQL::recordCountReady);
    m_database->queryRecordCount(0, nowMs + 1);

    if (countSpy.isEmpty()) {
        QVERIFY(countSpy.wait(3000));
    }
    QCOMPARE(countSpy.at(0).at(2).toLongLong(), static_cast<qint64>(2));
}

void TestSaveDataSql::alarmQueryAcknowledgeExport()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_database->enqueueAlarm(0, QStringLiteral("temperature high"));
    m_database->enqueueAlarm(1, QStringLiteral("comm lost"));
    m_database->flush();

    QSignalSpy querySpy(m_database, &SaveDataSQL::alarmsQueried);
    m_database->queryAlarms(nowMs - 60000, nowMs + 60000, 100);
    if (querySpy.isEmpty()) {
        QVERIFY(querySpy.wait(3000));
    }
    QCOMPARE(querySpy.count(), 1);

    const QVector<AlarmDbRecord> rows =
        querySpy.at(0).at(2).value<QVector<AlarmDbRecord>>();
    QCOMPARE(rows.size(), 2);
    QVERIFY(!rows.at(0).acknowledged);

    QSignalSpy ackSpy(m_database, &SaveDataSQL::alarmsAcknowledged);
    m_database->acknowledgeAlarm(0);
    if (ackSpy.isEmpty()) {
        QVERIFY(ackSpy.wait(3000));
    }
    QCOMPARE(ackSpy.at(0).at(0).toLongLong(), static_cast<qint64>(2));

    const QString csvPath = m_tempDir->filePath(QStringLiteral("alarm.csv"));
    QSignalSpy exportSpy(m_database, &SaveDataSQL::alarmExportFinished);
    m_database->exportAlarmsToCsv(nowMs - 60000, nowMs + 60000, csvPath);
    if (exportSpy.isEmpty()) {
        QVERIFY(exportSpy.wait(3000));
    }
    QVERIFY(exportSpy.at(0).at(0).toBool());
    QCOMPARE(exportSpy.at(0).at(1).toLongLong(), static_cast<qint64>(2));

    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY(content.contains(QStringLiteral("acknowledged")));
    QVERIFY(content.contains(QStringLiteral(",1")));
}

void TestSaveDataSql::backupAndVacuum()
{
    m_database->enqueueSample(1000, makeRound(1000, 2));
    m_database->flush();

    const QString backupPath = m_tempDir->filePath(QStringLiteral("backup.db"));

    QSignalSpy backupSpy(m_database, &SaveDataSQL::maintenanceFinished);
    m_database->backupDatabase(backupPath);
    if (backupSpy.isEmpty()) {
        QVERIFY(backupSpy.wait(5000));
    }
    QCOMPARE(backupSpy.count(), 1);
    QVERIFY(backupSpy.at(0).at(0).toBool());
    QVERIFY(QFile::exists(backupPath));

    QSignalSpy vacuumSpy(m_database, &SaveDataSQL::maintenanceFinished);
    m_database->vacuumDatabase();
    if (vacuumSpy.isEmpty()) {
        QVERIFY(vacuumSpy.wait(5000));
    }
    QVERIFY(vacuumSpy.at(0).at(0).toBool());
}

void TestSaveDataSql::restoreOverwritesCurrent()
{
    // 初始 2 条 → 备份 → 再写 2 条使当前库为 4 条 → 恢复备份 → 应回到 2 条
    m_database->enqueueSample(1000, makeRound(1000, 2));
    m_database->flush();

    const QString backupPath = m_tempDir->filePath(QStringLiteral("backup.db"));
    QSignalSpy backupSpy(m_database, &SaveDataSQL::maintenanceFinished);
    m_database->backupDatabase(backupPath);
    if (backupSpy.isEmpty()) {
        QVERIFY(backupSpy.wait(5000));
    }
    QVERIFY(backupSpy.at(0).at(0).toBool());

    m_database->enqueueSample(2000, makeRound(2000, 2));
    m_database->flush();

    QSignalSpy restoreSpy(m_database, &SaveDataSQL::maintenanceFinished);
    m_database->restoreDatabase(backupPath);
    if (restoreSpy.isEmpty()) {
        QVERIFY(restoreSpy.wait(5000));
    }
    QCOMPARE(restoreSpy.count(), 1);
    QVERIFY(restoreSpy.at(0).at(0).toBool());
    QCOMPARE(restoreSpy.at(0).at(1).toString(), QStringLiteral("restore"));

    QSignalSpy countSpy(m_database, &SaveDataSQL::recordCountReady);
    m_database->queryRecordCount(0, 5000);
    if (countSpy.isEmpty()) {
        QVERIFY(countSpy.wait(3000));
    }
    QCOMPARE(countSpy.at(0).at(2).toLongLong(), static_cast<qint64>(2));
}

QTEST_GUILESS_MAIN(TestSaveDataSql)

#include "test_savedatasql.moc"
