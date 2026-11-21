#ifndef HISTORYQUERYDIALOG_H
#define HISTORYQUERYDIALOG_H

#include "common/savedatasql.h"

#include <QDialog>
#include <QVector>

class ChannelModel;
class QComboBox;
class QCustomPlot;
class QDateTimeEdit;
class QLabel;
class QTableWidget;
class QVBoxLayout;

// 历史数据查询对话框：
//   - 选择起止时间与通道后，通过 queryRequested 交给 MainWidget 转发到数据库线程
//   - samplesQueried 回传后刷新表格与趋势图
//   - 实时数据表双击 / 数据存储卡片「查询」均可直接打开并带入范围
class HistoryQueryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryQueryDialog(ChannelModel *model, QWidget *parent = nullptr);

    // 以指定时间范围与通道打开对话框（channelIndex < 0 表示全部通道）
    void openWithRange(qint64 fromMs, qint64 toMs, int channelIndex);

    // 单次查询最大行数：MainWidget 与对话框共用，超过后提示用户缩小范围
    static constexpr int kQueryLimit = 5000;

signals:
    // 请求数据库线程查询采样记录
    void queryRequested(qint64 fromMs, qint64 toMs, int channelIndex);

public slots:
    // 数据库线程回传查询结果
    void onSamplesQueried(qint64 fromMs, qint64 toMs, const QVector<SampleChannelData> &rows);

private slots:
    void onQueryClicked();
    // 通道名称/数量变化后重建通道下拉
    void onChannelsChanged();

private:
    // 搭建控件
    void buildUi();
    void buildFilterBar(QVBoxLayout *layout);
    void buildPlot(QVBoxLayout *layout);
    // 按模型重建通道下拉，尽量保持当前选择
    void refreshChannelCombo();
    // 刷新结果表格
    void fillTable(const QVector<SampleChannelData> &rows);
    // 刷新趋势图
    void fillPlot(const QVector<SampleChannelData> &rows);
    // 表格行显示通道名
    QString channelName(int channel) const;

    ChannelModel *m_model = nullptr;

    QDateTimeEdit *m_fromEdit = nullptr;
    QDateTimeEdit *m_toEdit = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QLabel *m_countLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QCustomPlot *m_plot = nullptr;
};

#endif // HISTORYQUERYDIALOG_H
