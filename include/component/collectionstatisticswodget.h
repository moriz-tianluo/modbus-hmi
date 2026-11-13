#ifndef COLLECTIONSTATISTICSWODGET_H
#define COLLECTIONSTATISTICSWODGET_H

#include "base/BaseWidget.h"
#include "common/modbusrtu.h"

namespace Ui {
class CollectionStatisticsWodget;
}

// 采集统计卡片：累计采集轮次、成功率、超时/异常码/CRC 错误次数、RTT 与丢点率。
// 数据来源：MainWidget 把 SerialWorker 的 dataUpdated / pollFailed 转发到本类槽。
// 说明：统计粒度为“轮”（Poller 与 ModbusRtu 两层重试均已结束），
//       超时/异常码/CRC 计数记录的是该轮最终失败原因。
class CollectionStatisticsWodget : public BaseWidget
{
    Q_OBJECT

public:
    explicit CollectionStatisticsWodget(QWidget *parent = nullptr);
    ~CollectionStatisticsWodget();

    void languageChange() override;

public slots:
    // 一轮采集成功：累计总请求/成功数，并更新 RTT 累计与峰值
    void onRoundSuccess(qint64 rttMs);
    // 一轮采集失败：累计总请求/失败数，并按错误类型分类计数
    void onRoundFailed(ModbusError error);
    // 开始新一轮采集时清零所有统计
    void resetStatistics();

private:
    // 把当前统计值格式化后刷新到 UI 标签
    void refreshDisplay();

    Ui::CollectionStatisticsWodget *ui;

    // ---- 统计累计值 ----
    quint64 m_totalRounds = 0;      // 总请求轮次
    quint64 m_successRounds = 0;    // 成功轮次
    quint64 m_failedRounds = 0;     // 失败轮次（用于丢点率）
    quint64 m_timeoutCount = 0;     // 最终原因为超时的轮次
    quint64 m_exceptionCount = 0;   // 最终原因为异常码的轮次
    quint64 m_crcErrorCount = 0;    // 最终原因为 CRC 校验错的轮次
    qint64 m_rttSumMs = 0;          // 成功轮次 RTT 累计（用于平均）
    qint64 m_rttPeakMs = 0;         // 成功轮次 RTT 峰值
};

#endif // COLLECTIONSTATISTICSWODGET_H
