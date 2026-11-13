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

private:
    // 建表 + 建索引 + 设置 WAL
    bool createTables(QString *errorString);
    // 把缓冲区中的采样行事务写入
    bool commitSamples(QString *errorString);

    QSqlDatabase m_db;
    QString m_connectionName;
    bool m_opened = false;

    QVector<SampleChannelData> m_buffer;  // 待写入的采样行
    int m_pendingRounds = 0;              // 缓冲区累计的采样轮次
};

#endif // SAVEDATASQL_H
