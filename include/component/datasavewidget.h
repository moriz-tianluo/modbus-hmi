#ifndef DATASAVEWIDGET_H
#define DATASAVEWIDGET_H

#include "base/BaseWidget.h"

#include <QPair>

namespace Ui {
class DataSaveWidget;
}

// 数据存储卡片：按时间范围统计采样记录数，并把记录导出为 CSV。
// 本控件只负责界面与时间范围换算；真正的数据库读写通过信号投递给数据库线程，
// 结果再以槽的形式回流到本控件刷新显示。
class DataSaveWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit DataSaveWidget(QWidget *parent = nullptr);
    ~DataSaveWidget();

    void languageChange() override;

    // 顶部工具条/菜单「导出采集数据」入口：等价于点击本卡片的导出按钮
    void requestExport();

signals:
    // 请求统计时间范围 [fromMs, toMs] 内的采样记录数
    void recordCountRequested(qint64 fromMs, qint64 toMs);
    // 请求打开历史查询对话框（带当前时间范围）
    void historyQueryRequested(qint64 fromMs, qint64 toMs);
    // 请求把时间范围 [fromMs, toMs] 内的采样记录导出到 CSV
    // fieldMask 为导出列位掩码（SaveDataSQL::ExportField 组合）
    void exportRequested(qint64 fromMs, qint64 toMs, const QString &filePath,
                         int fieldMask);

public slots:
    // 数据库线程回传：记录数查询结果
    void onRecordCountReady(qint64 fromMs, qint64 toMs, qint64 count);
    // 数据库线程回传：CSV 导出结果
    void onExportFinished(bool ok, qint64 rows, const QString &filePath, const QString &errorString);

private slots:
    void onQueryClicked();
    void onExportClicked();
    // 时间范围下拉切换：选中「自定义…」时弹出起止时间设置
    void onTimeRangeChanged(int index);

private:
    // 按当前语言重建时间范围下拉项
    void initTimeRangeItems();
    // 刷新数据库文件信息（文件名 + 大小）
    void refreshDatabaseInfo();
    // 按 m_lastCount 刷新记录数文本
    void refreshRecordCountText();
    // 把当前下拉选项换算成 [from, to] 毫秒时间戳
    QPair<qint64, qint64> currentTimeRange() const;
    // 弹出自定义时间范围设置，取消时返回 false
    bool askCustomRange();
    // 弹出导出字段勾选，取消时返回 false；mask 为 SaveDataSQL::ExportField 组合
    bool askExportFields(int *mask);

private:
    Ui::DataSaveWidget *ui;

    qint64 m_lastCount = 0;  // 最近一次查询到的记录数

    static constexpr int kCustomRangeIndex = 4;  // 「自定义…」在下拉中的固定位置
    qint64 m_customFromMs = 0;                   // 自定义范围起点
    qint64 m_customToMs = 0;                     // 自定义范围终点
};

#endif // DATASAVEWIDGET_H
