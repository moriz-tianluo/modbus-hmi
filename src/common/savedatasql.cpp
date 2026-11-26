#include "savedatasql.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

SaveDataSQL::SaveDataSQL(QObject *parent)
    : QObject{parent}
{
    // 跨线程队列连接需要注册自定义类型
    qRegisterMetaType<SampleChannelData>("SampleChannelData");
    qRegisterMetaType<QVector<SampleChannelData>>("QVector<SampleChannelData>");
    qRegisterMetaType<AlarmDbRecord>("AlarmDbRecord");
    qRegisterMetaType<QVector<AlarmDbRecord>>("QVector<AlarmDbRecord>");
}

SaveDataSQL::~SaveDataSQL()
{
    closeDatabase();
}

bool SaveDataSQL::open(const QString &filePath, QString *errorString)
{
    // 先释放已有连接，避免重复打开
    closeDatabase();

    // 确保目录存在（程序目录/data/hmi.db）
    const QFileInfo info(filePath);
    if (!info.dir().exists()) {
        QDir().mkpath(info.dir().absolutePath());
    }

    m_connectionName = QStringLiteral("hmi_savedata");
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(filePath);

    if (!m_db.open()) {
        if (errorString != nullptr) {
            *errorString = m_db.lastError().text();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    // WAL + NORMAL：批量写入更快，掉电最多丢失最后一个事务
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    if (!createTables(errorString)) {
        return false;
    }

    m_dbPath = filePath;
    m_opened = true;
    return true;
}

void SaveDataSQL::openDatabase(const QString &filePath)
{
    QString error;
    const bool ok = open(filePath, &error);
    emit databaseOpened(ok, error);
}

void SaveDataSQL::closeDatabase()
{
    if (!m_opened && m_connectionName.isEmpty()) {
        return;
    }

    // 关闭前把剩余数据落盘
    flush();

    if (m_db.isOpen()) {
        m_db.close();
    }

    const QString name = m_connectionName;
    m_db = QSqlDatabase();
    if (!name.isEmpty()) {
        QSqlDatabase::removeDatabase(name);
    }

    m_connectionName.clear();
    m_opened = false;
}

bool SaveDataSQL::createTables(QString *errorString)
{
    QSqlQuery query(m_db);

    // 窄表：每通道一行，通道数变化无需改表
    const QString sampleSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS sample ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts INTEGER NOT NULL,"
        " channel INTEGER NOT NULL,"
        " raw INTEGER NOT NULL,"
        " value REAL NOT NULL)");
    if (!query.exec(sampleSql)) {
        *errorString = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_sample_ts ON sample(ts)"))) {
        *errorString = query.lastError().text();
        return false;
    }

    // 报警记录：时间、等级（0=报警，1=故障）、描述、确认状态
    const QString alarmSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS alarm ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts INTEGER NOT NULL,"
        " level INTEGER NOT NULL,"
        " message TEXT NOT NULL,"
        " acknowledged INTEGER NOT NULL DEFAULT 0)");
    if (!query.exec(alarmSql)) {
        *errorString = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_alarm_ts ON alarm(ts)"))) {
        *errorString = query.lastError().text();
        return false;
    }

    return applyMigrations(errorString);
}

bool SaveDataSQL::applyMigrations(QString *errorString)
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        *errorString = query.lastError().text();
        return false;
    }

    const int version = query.value(0).toInt();
    if (version >= kSchemaVersion) {
        return true;
    }

    // v2：按通道筛选走 (channel, ts) 复合索引，避免全表扫描
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_sample_channel_ts ON sample(channel, ts)"))) {
        *errorString = query.lastError().text();
        return false;
    }

    // v2：老库补齐 alarm.acknowledged 列
    bool hasAcknowledged = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(alarm)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QStringLiteral("acknowledged")) {
                hasAcknowledged = true;
                break;
            }
        }
    }
    if (!hasAcknowledged) {
        if (!query.exec(QStringLiteral(
                "ALTER TABLE alarm ADD COLUMN acknowledged INTEGER NOT NULL DEFAULT 0"))) {
            *errorString = query.lastError().text();
            return false;
        }
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        *errorString = query.lastError().text();
        return false;
    }
    return true;
}

void SaveDataSQL::enqueueSample(qint64 timestampMs, const QVector<SampleChannelData> &channels)
{
    if (!m_opened || channels.isEmpty()) {
        return;
    }

    // 本轮所有通道共用同一时间戳
    m_buffer.reserve(m_buffer.size() + channels.size());
    for (SampleChannelData row : channels) {
        row.ts = timestampMs;
        m_buffer.append(row);
    }
    ++m_pendingRounds;

    if (m_pendingRounds >= kBatchSize) {
        flush();
    }
}

void SaveDataSQL::enqueueAlarm(int level, const QString &message)
{
    if (!m_opened) {
        return;
    }

    QSqlQuery query(m_db);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO alarm(ts, level, message) VALUES(?, ?, ?)"))) {
        emit databaseError(query.lastError().text());
        return;
    }

    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(level);
    query.addBindValue(message);

    if (!query.exec()) {
        emit databaseError(query.lastError().text());
    }
}

void SaveDataSQL::flush()
{
    if (!m_opened || m_buffer.isEmpty()) {
        return;
    }

    QString error;
    if (!commitSamples(&error)) {
        emit databaseError(error);
        // 写入失败时清空缓冲，避免内存无限增长
        m_buffer.clear();
        m_pendingRounds = 0;
        return;
    }

    const int rounds = m_pendingRounds;
    m_buffer.clear();
    m_pendingRounds = 0;
    emit samplesCommitted(rounds);
}

bool SaveDataSQL::commitSamples(QString *errorString)
{
    if (!m_db.transaction()) {
        *errorString = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO sample(ts, channel, raw, value) VALUES(?, ?, ?, ?)"))) {
        *errorString = query.lastError().text();
        m_db.rollback();
        return false;
    }

    for (const SampleChannelData &row : m_buffer) {
        query.bindValue(0, row.ts);
        query.bindValue(1, row.channel);
        query.bindValue(2, static_cast<int>(row.raw));
        query.bindValue(3, row.value);

        if (!query.exec()) {
            *errorString = query.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        *errorString = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

void SaveDataSQL::queryRecordCount(qint64 fromMs, qint64 toMs)
{
    qint64 count = 0;

    if (m_opened) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM sample WHERE ts >= ? AND ts <= ?"));
        query.addBindValue(fromMs);
        query.addBindValue(toMs);

        if (query.exec() && query.next()) {
            count = query.value(0).toLongLong();
        }
    }

    // 无论成功与否都回传一次结果，避免界面一直等待
    emit recordCountReady(fromMs, toMs, count);
}

void SaveDataSQL::querySamples(qint64 fromMs, qint64 toMs, int channel, int limit)
{
    QVector<SampleChannelData> rows;

    if (m_opened) {
        QString sql = QStringLiteral(
            "SELECT ts, channel, raw, value FROM sample WHERE ts >= ? AND ts <= ?");
        if (channel >= 0) {
            sql += QStringLiteral(" AND channel = ?");
        }
        sql += QStringLiteral(" ORDER BY ts ASC, channel ASC LIMIT %1")
                   .arg(qMax(1, limit));

        QSqlQuery query(m_db);
        query.prepare(sql);
        query.addBindValue(fromMs);
        query.addBindValue(toMs);
        if (channel >= 0) {
            query.addBindValue(channel);
        }

        if (query.exec()) {
            while (query.next()) {
                SampleChannelData row;
                row.ts = query.value(0).toLongLong();
                row.channel = query.value(1).toInt();
                row.raw = static_cast<quint16>(query.value(2).toInt());
                row.value = query.value(3).toDouble();
                rows.append(row);
            }
        }
    }

    // 无论成功与否都回传一次，避免对话框一直等待
    emit samplesQueried(fromMs, toMs, rows);
}

void SaveDataSQL::exportSamplesToCsv(qint64 fromMs, qint64 toMs, const QString &filePath,
                                     int fieldMask)
{
    // 未勾选任何列时退回全字段，避免导出空文件
    const int mask = (fieldMask == 0) ? 0x0F : fieldMask;

    // 数据库未就绪：errorString 置空，由界面给出本地化提示
    if (!m_opened) {
        emit exportFinished(false, 0, filePath, QString());
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFinished(false, 0, filePath, file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);  // 带 BOM，Excel 打开中文不乱码

    // 表头与勾选字段保持一致
    QStringList headers;
    if (mask & ExportTime) {
        headers << QStringLiteral("ts") << QStringLiteral("datetime");
    }
    if (mask & ExportChannel) {
        headers << QStringLiteral("channel");
    }
    if (mask & ExportRaw) {
        headers << QStringLiteral("raw");
    }
    if (mask & ExportValue) {
        headers << QStringLiteral("value");
    }
    stream << headers.join(QLatin1Char(',')) << '\n';

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT ts, channel, raw, value FROM sample"
        " WHERE ts >= ? AND ts <= ? ORDER BY ts, channel"));
    query.addBindValue(fromMs);
    query.addBindValue(toMs);

    if (!query.exec()) {
        const QString error = query.lastError().text();
        file.close();
        emit exportFinished(false, 0, filePath, error);
        return;
    }

    qint64 rows = 0;
    while (query.next()) {
        const qint64 ts = query.value(0).toLongLong();
        const int channel = query.value(1).toInt();
        const int raw = query.value(2).toInt();
        const double value = query.value(3).toDouble();
        const QString datetime = QDateTime::fromMSecsSinceEpoch(ts)
                                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

        QStringList columns;
        if (mask & ExportTime) {
            columns << QString::number(ts) << datetime;
        }
        if (mask & ExportChannel) {
            columns << QString::number(channel);
        }
        if (mask & ExportRaw) {
            columns << QString::number(raw);
        }
        if (mask & ExportValue) {
            columns << QString::number(value, 'f', 3);
        }

        stream << columns.join(QLatin1Char(',')) << '\n';
        ++rows;
    }
    file.close();

    emit exportFinished(true, rows, filePath, QString());
}

void SaveDataSQL::purgeOlderThan(int days)
{
    if (!m_opened || days <= 0) {
        return;
    }

    const qint64 cutoffMs =
        QDateTime::currentMSecsSinceEpoch() - static_cast<qint64>(days) * 86400000LL;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM sample WHERE ts < ?"));
    query.addBindValue(cutoffMs);
    if (!query.exec()) {
        emit databaseError(query.lastError().text());
        return;
    }
    const qint64 rows = query.numRowsAffected();

    // 报警记录同步清理，避免库无限增长
    QSqlQuery alarmQuery(m_db);
    alarmQuery.prepare(QStringLiteral("DELETE FROM alarm WHERE ts < ?"));
    alarmQuery.addBindValue(cutoffMs);
    alarmQuery.exec();

    emit purgedOlderThan(days, rows);
}

void SaveDataSQL::queryAlarms(qint64 fromMs, qint64 toMs, int limit)
{
    QVector<AlarmDbRecord> rows;
    QString error;
    if (m_opened) {
        selectAlarms(fromMs, toMs, limit, &rows, &error);
        if (!error.isEmpty()) {
            emit databaseError(error);
        }
    }
    emit alarmsQueried(fromMs, toMs, rows);
}

bool SaveDataSQL::selectAlarms(qint64 fromMs, qint64 toMs, int limit,
                               QVector<AlarmDbRecord> *rows, QString *errorString)
{
    QString sql = QStringLiteral(
        "SELECT id, ts, level, message, acknowledged FROM alarm"
        " WHERE ts >= ? AND ts <= ? ORDER BY ts ASC, id ASC");
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    }

    QSqlQuery query(m_db);
    if (!query.prepare(sql)) {
        *errorString = query.lastError().text();
        return false;
    }
    query.addBindValue(fromMs);
    query.addBindValue(toMs);
    if (!query.exec()) {
        *errorString = query.lastError().text();
        return false;
    }

    while (query.next()) {
        AlarmDbRecord record;
        record.id = query.value(0).toLongLong();
        record.ts = query.value(1).toLongLong();
        record.level = query.value(2).toInt();
        record.message = query.value(3).toString();
        record.acknowledged = query.value(4).toInt() != 0;
        rows->append(record);
    }
    return true;
}

void SaveDataSQL::acknowledgeAlarm(qint64 id)
{
    if (!m_opened) {
        emit alarmsAcknowledged(0);
        return;
    }

    QSqlQuery query(m_db);
    if (id > 0) {
        query.prepare(QStringLiteral("UPDATE alarm SET acknowledged = 1 WHERE id = ?"));
        query.addBindValue(id);
    } else {
        query.prepare(QStringLiteral("UPDATE alarm SET acknowledged = 1"
                                     " WHERE acknowledged = 0"));
    }

    if (!query.exec()) {
        emit databaseError(query.lastError().text());
        emit alarmsAcknowledged(0);
        return;
    }
    emit alarmsAcknowledged(query.numRowsAffected());
}

void SaveDataSQL::exportAlarmsToCsv(qint64 fromMs, qint64 toMs, const QString &filePath)
{
    if (!m_opened) {
        emit alarmExportFinished(false, 0, filePath, QString());
        return;
    }

    QVector<AlarmDbRecord> rows;
    QString error;
    if (!selectAlarms(fromMs, toMs, 0, &rows, &error)) {
        emit alarmExportFinished(false, 0, filePath, error);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit alarmExportFinished(false, 0, filePath, file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << QStringLiteral("ts,datetime,level,message,acknowledged\n");

    for (const AlarmDbRecord &record : rows) {
        QString message = record.message;
        message.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        if (message.contains(QLatin1Char(',')) || message.contains(QLatin1Char('"'))) {
            message = QLatin1Char('"') + message + QLatin1Char('"');
        }
        stream << record.ts << QLatin1Char(',')
               << QDateTime::fromMSecsSinceEpoch(record.ts)
                      .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
               << QLatin1Char(',') << record.level << QLatin1Char(',')
               << message << QLatin1Char(',') << (record.acknowledged ? 1 : 0)
               << QLatin1Char('\n');
    }
    file.close();

    emit alarmExportFinished(true, rows.size(), filePath, QString());
}

void SaveDataSQL::vacuumDatabase()
{
    if (!m_opened) {
        emit maintenanceFinished(false, QStringLiteral("vacuum"), QString());
        return;
    }

    // 先把缓冲区落盘，避免 VACUUM 与未提交事务交叉
    flush();

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("VACUUM"))) {
        emit maintenanceFinished(false, QStringLiteral("vacuum"),
                                 query.lastError().text());
        return;
    }
    emit maintenanceFinished(true, QStringLiteral("vacuum"), QString());
}

void SaveDataSQL::backupDatabase(const QString &targetPath)
{
    if (!m_opened) {
        emit maintenanceFinished(false, QStringLiteral("backup"), QString());
        return;
    }

    flush();

    // VACUUM INTO 要求目标文件不存在
    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        emit maintenanceFinished(false, QStringLiteral("backup"),
                                 QStringLiteral("cannot overwrite target file"));
        return;
    }
    ensureDirectory(targetPath);

    QString escapedPath = targetPath;
    escapedPath.replace(QLatin1Char('\''), QStringLiteral("''"));

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("VACUUM INTO '%1'").arg(escapedPath))) {
        emit maintenanceFinished(false, QStringLiteral("backup"),
                                 query.lastError().text());
        return;
    }
    emit maintenanceFinished(true, QStringLiteral("backup"), QString());
}

void SaveDataSQL::restoreDatabase(const QString &sourcePath)
{
    if (m_dbPath.isEmpty()) {
        emit maintenanceFinished(false, QStringLiteral("restore"), QString());
        return;
    }
    if (!QFile::exists(sourcePath)) {
        emit maintenanceFinished(false, QStringLiteral("restore"),
                                 QStringLiteral("source file not found"));
        return;
    }
    if (QFileInfo(sourcePath).absoluteFilePath()
        == QFileInfo(m_dbPath).absoluteFilePath()) {
        emit maintenanceFinished(false, QStringLiteral("restore"),
                                 QStringLiteral("source is the current database"));
        return;
    }

    // 关闭当前连接（含 flush 落盘），才能安全替换数据库文件
    closeDatabase();

    // 删除当前库及其 WAL / SHM，避免旧日志干扰恢复后的库
    const QStringList suffixes = {QString(), QStringLiteral("-wal"),
                                  QStringLiteral("-shm")};
    for (const QString &suffix : suffixes) {
        const QString target = m_dbPath + suffix;
        if (QFile::exists(target) && !QFile::remove(target)) {
            emit maintenanceFinished(false, QStringLiteral("restore"),
                                     QStringLiteral("cannot remove %1").arg(target));
            return;
        }
    }

    ensureDirectory(m_dbPath);
    if (!QFile::copy(sourcePath, m_dbPath)) {
        emit maintenanceFinished(false, QStringLiteral("restore"),
                                 QStringLiteral("cannot copy source file"));
        return;
    }

    QString error;
    if (!open(m_dbPath, &error)) {
        emit maintenanceFinished(false, QStringLiteral("restore"), error);
        return;
    }
    emit maintenanceFinished(true, QStringLiteral("restore"), QString());
}

void SaveDataSQL::queryDatabaseSize()
{
    if (!m_opened) {
        emit databaseSizeReady(0);
        return;
    }
    emit databaseSizeReady(QFileInfo(m_db.databaseName()).size());
}

bool SaveDataSQL::ensureDirectory(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.dir().exists()) {
        return true;
    }
    return QDir().mkpath(info.dir().absolutePath());
}
