#ifndef ALARMANDFAULTWIDGET_H
#define ALARMANDFAULTWIDGET_H

#include "base/BaseWidget.h"

#include <QDateTime>
#include <QVector>

class QLabel;
class QWidget;

namespace Ui {
class AlarmAndFaultWidget;
}

// 报警/故障等级：Alarm=报警（预警色），Fault=故障（危险色）
// QSS 通过 QLabel 的 role 属性区分（tagAlarm / tagFault）
enum class AlarmLevel {
    Alarm,
    Fault
};

// 单条报警记录
struct AlarmRecord {
    QDateTime time;    // 触发时间
    AlarmLevel level;  // 等级
    QString message;   // 已国际化后的描述文本
};

// 报警与故障卡片：
//   - 使用定位：采集链路的异常统一出口（串口错误、轮询失败、断线重连），后续通道越限也走这里
//   - 展示策略：卡片只显示最新 3 条，右上角角标显示总条数，“查看全部”弹窗列出全部记录
//   - 去抖策略：与上一条等级/内容相同且间隔小于 5s 时只刷新时间，不重复追加
//   - 容量上限：最多保留 200 条，超出后丢弃最旧记录
class AlarmAndFaultWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit AlarmAndFaultWidget(QWidget *parent = nullptr);
    ~AlarmAndFaultWidget();

    void languageChange() override;

    // 更新底部规则提示（连续越限次数等），由 MainWidget 在规则变化后调用
    void setParamTip(const QString &text);

public slots:
    // 追加一条报警/故障记录（最新记录排在最前）
    void addAlarm(AlarmLevel level, const QString &message);
    // 清空全部记录
    void clearAlarms();

signals:
    // 真正追加了一条新记录（被去抖的重复项不发射），供数据库等旁路记录
    void alarmRecordAdded(int level, const QString &message);

private:
    // 按当前记录刷新前三行、角标与总数
    void refreshList();
    // 用一条记录填充指定行（rowIndex: 0~2）
    void updateRow(int rowIndex, const AlarmRecord &record);
    // 依据等级重设标签的 role 属性，使 QSS 中 tagAlarm/tagFault 样式生效
    void applyTagStyle(QLabel *tagLabel, AlarmLevel level);
    // 弹窗展示全部记录
    void showAllRecords();

private:

    static constexpr int kMaxRecords = 200;   // 记录保留上限
    static constexpr int kDisplayRows = 3;    // 卡片固定展示行数
    static constexpr int kDebounceMs = 5000;  // 相同内容去抖窗口（毫秒）

    Ui::AlarmAndFaultWidget *ui;

    // 行控件指针（构造时按 ui 成员填充，便于循环刷新）
    QWidget *m_itemWidgets[kDisplayRows] = {};
    QLabel *m_timeLabels[kDisplayRows] = {};
    QLabel *m_tagLabels[kDisplayRows] = {};
    QLabel *m_msgLabels[kDisplayRows] = {};

    QVector<AlarmRecord> m_records;  // index 0 为最新记录
};

#endif // ALARMANDFAULTWIDGET_H
