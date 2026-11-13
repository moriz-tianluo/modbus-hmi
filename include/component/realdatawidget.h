#ifndef REALDATAWIDGET_H
#define REALDATAWIDGET_H

#include "base/BaseWidget.h"

class ChannelModel;

namespace Ui {
class RealDataWidget;
}

// 实时数据表格卡片：展示所有通道的当前采集值、原始值、工程值、单位等。
// MainWidget 注入 ChannelModel 后，监听 channelValueChanged 信号逐行刷新。
class RealDataWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit RealDataWidget(QWidget *parent = nullptr);
    ~RealDataWidget();

    void languageChange() override;

    // MainWidget 初始化时注入 ChannelModel 指针；本类不持有所有权
    void setChannelModel(ChannelModel *model);

public slots:
    // ChannelModel::channelValueChanged 槽——单通道值变化时更新对应行
    void onChannelValueChanged(int index, double value);
    // ChannelModel::channelsChanged 槽——通道名称/单位/数量变化后重建表格
    void onChannelsChanged();
    // ChannelModel::channelEnabledChanged 槽——启用开关联动：整行置灰 + 状态列提示
    void onChannelEnabledChanged(int index, bool enabled);
    // 曲线图例点击后选中对应行；再次点击同一行则取消高亮
    void selectChannelRow(int index);
    // 一轮采集的往返耗时（同一轮内所有通道共用），刷新 RTT 列
    void setRoundTripTime(int milliseconds);
    // 数据库线程回传记录数：仅接受全量统计（fromMs == 0）用于底部脚注
    void onRecordCountReady(qint64 fromMs, qint64 toMs, qint64 count);

signals:
    // 用户选中某一行时发出（-1 表示取消选中），用于联动曲线高亮
    void channelSelected(int index);
    // 双击某一行：请求查看该通道的历史曲线/数据
    void historyRequested(int index);

private slots:
    // 单击单元格：同一行再次单击即取消高亮
    void onCellClicked(int row, int column);
    // 双击单元格：打开历史查询对话框
    void onCellDoubleClicked(int row, int column);

private:
    // 初始化表格列宽与行高
    void initTable();
    // 设置表头文字
    void setHeaderText(int column, const QString &text);
    // 清空表格并按 m_channelModel 重建所有行
    void rebuildTable();
    // 按启用状态刷新整行前景色与状态列
    void applyRowEnabledStyle(int row, bool enabled);
    // 按通道报警阈值刷新状态列文字与整行红色告警样式
    void updateAlarmStyle(int index);
    // 按 m_totalRecords 刷新底部脚注
    void refreshDbTip();
    // 取消高亮并通知曲线恢复常态
    void clearChannelHighlight();

    static constexpr int kColumnCount = 8;

    Ui::RealDataWidget *ui;
    ChannelModel *m_channelModel = nullptr;
    int m_highlightRow = -1;  // 当前高亮行，-1 表示无高亮
    qint64 m_totalRecords = 0;  // 数据库总采样记录数（用于底部脚注）
};

#endif // REALDATAWIDGET_H
