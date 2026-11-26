#ifndef SAVEDATASQL_H
#define SAVEDATASQL_H

#include <QMetaType>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// 单通道采样值（原始寄存器值 + 工程值）
struct SampleChannelData {
    qint64 ts = 0;        // 采样时间（Unix 毫秒）
    int channel = 0;      // 通道序号
    quint16 raw = 0;      // 原始寄存器值
    double value = 0.0;   // 工程值
};

Q_DECLARE_METATYPE(SampleChannelData)

// 报警记录（含确认状态）
struct AlarmDbRecord {
    qint64 id = 0;             // 数据库主键
    qint64 ts = 0;             // 发生时间（Unix 毫秒）
    int level = 0;             // 0=报警，1=故障
    QString message;           // 描述
    bool acknowledged = false; // 是否已确认
};

Q_DECLARE_METATYPE(AlarmDbRecord)
Q_DECLARE_METATYPE(QVector<AlarmDbRecord>)

// SQLite 数据存储服务：
//   - 独立线程运行（由外部 moveToThread + QThread 驱动），UI 只通过队列连接投递数据
//   - 采样数据先进入内存缓冲，累计 50 个采样轮次后一次性事务提交（WAL 模式）
//   - 报警记录低频且重要，到达即写入
//   - 程序退出前由外部调用 flush() 落盘剩余数据，再 closeDatabase()
// 线程约束：所有方法必须在数据库线程内执行，成员无需加锁。
class SaveDataSQL : public QObject
{
    Q_OBJECT

public:
    explicit SaveDataSQL(QObject *parent = nullptr);
    ~SaveDataSQL() override;

    static constexpr int kBatchSize = 50;  // 每 50 个采样轮次提交一次
    static constexpr int kSchemaVersion = 2;  // PRAGMA user_version 当前版本

    // CSV 导出字段位掩码，可组合使用
    enum ExportField {
        ExportTime = 0x1,     // 时间戳 + 可读时间
        ExportChannel = 0x2,  // 通道序号
        ExportRaw = 0x4,      // 原始寄存器值
        ExportValue = 0x8,    // 工程值
    };

public slots:
    // 打开/创建数据库文件（会自动创建目录与表结构）
    void openDatabase(const QString &filePath);
    // 先 flush 再关闭连接
    void closeDatabase();
    // 投递一轮采样：同一轮内所有通道共用 timestampMs
    void enqueueSample(qint64 timestampMs, const QVector<SampleChannelData> &channels);
    // 投递一条报警/故障（到达即写库）
    void enqueueAlarm(int level, const QString &message);
    // 强制把缓冲区剩余采样写入数据库
    void flush();
    // 统计时间范围 [fromMs, toMs] 内的采样记录数，结果通过 recordCountReady 回传
    void queryRecordCount(qint64 fromMs, qint64 toMs);
    // 查询时间范围 [fromMs, toMs] 内的采样记录，channel < 0 表示全部通道；
    // limit 为最大返回行数（按时间升序），结果通过 samplesQueried 回传
    void querySamples(qint64 fromMs, qint64 toMs, int channel, int limit);
    // 把时间范围 [fromMs, toMs] 内的采样记录导出为 CSV，结果通过 exportFinished 回传。
    // fieldMask 为导出列位掩码：1=时间戳 2=通道 4=原始值 8=工程值
    void exportSamplesToCsv(qint64 fromMs, qint64 toMs, const QString &filePath,
                            int fieldMask);
    // 删除早于 days 天前的采样与报警记录，days<=0 时不做任何处理
    void purgeOlderThan(int days);

    // ---- 报警闭环 ----
    // 查询时间范围 [fromMs, toMs] 内的报警记录（按时间升序，最多 limit 行）
    void queryAlarms(qint64 fromMs, qint64 toMs, int limit);
    // 确认（acknowledge）指定 id 的报警；id<=0 表示全部未确认记录
    void acknowledgeAlarm(qint64 id);
    // 把报警记录导出为 CSV（包含确认状态列）
    void exportAlarmsToCsv(qint64 fromMs, qint64 toMs, const QString &filePath);

    // ---- 维护 ----
    // 整理数据库碎片（VACUUM），结果通过 maintenanceFinished 回传
    void vacuumDatabase();
    // 备份数据库到 targetPath（VACUUM INTO，目标文件存在时会先删除）
    void backupDatabase(const QString &targetPath);
    // 用 sourcePath 覆盖当前数据库（关闭连接 → 替换文件 → 重新打开）
    void restoreDatabase(const QString &sourcePath);
    // 当前数据库文件大小（字节），通过 databaseSizeReady 回传
    void queryDatabaseSize();

signals:
    void databaseOpened(bool ok, const QString &errorString);
    void databaseError(const QString &errorString);
    // 一次批量提交完成：rounds = 本次提交的采样轮次
    void samplesCommitted(int rounds);
    // 记录数查询结果（回传请求的时间范围，便于调用方校验是否为自己发起的请求）
    void recordCountReady(qint64 fromMs, qint64 toMs, qint64 count);
    // 采样查询结果（rows 按时间升序，最多 limit 行）
    void samplesQueried(qint64 fromMs, qint64 toMs, const QVector<SampleChannelData> &rows);
    // CSV 导出结果：ok=false 时 errorString 说明失败原因（可能为空，表示数据库未就绪）
    void exportFinished(bool ok, qint64 rows, const QString &filePath, const QString &errorString);
    // 自动清理结果：rows = 删除的采样行数，days 为触发清理的保留天数
    void purgedOlderThan(int days, qint64 rows);
    // 报警查询结果
    void alarmsQueried(qint64 fromMs, qint64 toMs, const QVector<AlarmDbRecord> &rows);
    // 报警确认完成：rows = 本次确认的行数
    void alarmsAcknowledged(qint64 rows);
    // 报警 CSV 导出结果
    void alarmExportFinished(bool ok, qint64 rows, const QString &filePath,
                             const QString &errorString);
    // 维护操作结果（VACUUM / 备份）：ok=false 时 errorString 说明原因
    void maintenanceFinished(bool ok, const QString &action, const QString &errorString);
    // 数据库文件大小（字节）
    void databaseSizeReady(qint64 bytes);

private:
    // 打开/创建数据库（不发信号），供 openDatabase 与 restoreDatabase 复用
    bool open(const QString &filePath, QString *errorString);
    // 建表 + 建索引 + 设置 WAL
    bool createTables(QString *errorString);
    // 按 PRAGMA user_version 升级表结构（索引补充、alarm.acknowledged 列）
    bool applyMigrations(QString *errorString);
    // 把缓冲区中的采样行事务写入
    bool commitSamples(QString *errorString);
    // 按时间范围读取报警（queryAlarms / 导出共用）
    bool selectAlarms(qint64 fromMs, qint64 toMs, int limit, QVector<AlarmDbRecord> *rows,
                      QString *errorString);
    // 确保目录存在
    static bool ensureDirectory(const QString &filePath);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_dbPath;  // 当前数据库文件路径，恢复时用作被替换目标
    bool m_opened = false;

    QVector<SampleChannelData> m_buffer;  // 待写入的采样行
    int m_pendingRounds = 0;              // 缓冲区累计的采样轮次
};

#endif // SAVEDATASQL_H
