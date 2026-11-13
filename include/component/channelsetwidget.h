#ifndef CHANNELSETWIDGET_H
#define CHANNELSETWIDGET_H

#include "base/BaseWidget.h"

#include <QIcon>

class ChannelModel;
class QTableWidgetItem;

namespace Ui {
class ChannelSetWidget;
}

// 通道配置卡片：展示 ChannelModel 中的通道列表、启用/禁用、当前值。
// 还包含导入/导出 JSON 配置、缓存点数选择等辅助控件。
// MainWidget 在初始化时调用 setChannelModel 注入数据模型。
class ChannelSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ChannelSetWidget(QWidget *parent = nullptr);
    ~ChannelSetWidget();

    void languageChange() override;

    // MainWidget 初始化时注入 ChannelModel 指针；本类不持有所有权
    void setChannelModel(ChannelModel *model);

    // 落库开关/缓冲点数的读写，供 ProjectConfig 持久化时回填与采集
    bool saveToDatabase() const;
    void setSaveToDatabase(bool enabled);
    int bufferSize() const;
    void setBufferSizeValue(int size);

signals:
    // 落库开关变化：false 时 MainWidget 只显示不写库
    void saveToDatabaseToggled(bool enabled);
    // 导入/导出通道配置成功后发出，用于记忆 JSON 路径
    void configPathChanged(const QString &path);

public slots:
    // ChannelModel::channelsChanged 槽——整个通道列表重建后刷新表格
    void onChannelsChanged();
    // ChannelModel::channelValueChanged 槽——单通道值变化时更新对应行的当前值列
    void onChannelValueChanged(int index, double value);
    // 菜单「全选/全不选通道」入口
    void setAllChannelsEnabled(bool enabled);

private slots:
    void onSelectAllClicked();
    void onChannelItemChanged(QTableWidgetItem *item);
    void onBufferChanged(int index);
    void onSaveToDatabaseToggled(bool checked);
    void onImportClicked();
    void onExportClicked();

private:
    // 初始化通道表格的列宽与表头高度
    void initTable();
    // 连接本卡片内的按钮/下拉/勾选框
    void initConnections();

    // 设置指列表头文字
    void setHeaderText(int column, const QString &text);
    // 清空表格并按 m_channelModel 重建所有行
    void rebuildTable();
    // 未启用通道整行变灰，启用通道恢复默认颜色
    void applyRowStyle(int row, bool enabled);
    // 刷新底部「N 通道 · M 已启用」汇总
    void refreshSummary();
    // 全选按钮文字随状态切换：未全部启用显示「全选」，全部启用显示「全不选」
    void updateSelectAllButtonText();
    // 缓冲下拉跟随模型当前缓存点数
    void syncBufferCombo();
    // 生成通道色点图标，与曲线颜色一一对应
    QIcon channelColorIcon(int index) const;

private:
    Ui::ChannelSetWidget *ui;
    ChannelModel *m_channelModel = nullptr;

    bool m_saveToDatabase = true;  // 是否落库
    bool m_updating = false;       // 表格重建/样式刷新期间屏蔽 itemChanged
};

#endif // CHANNELSETWIDGET_H
