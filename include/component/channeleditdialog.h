#ifndef CHANNELEDITDIALOG_H
#define CHANNELEDITDIALOG_H

#include "common/channelmodel.h"

#include <QColor>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

// 通道管理对话框：新增/编辑/删除单个通道。
// 编辑项：名称、寄存器地址、数据类型、单位、换算系数、量程、小数位、颜色、启用状态。
// 直接读写 ChannelModel，保存后由 channelsChanged 驱动各显示控件刷新。
class ChannelEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChannelEditDialog(ChannelModel *model, QWidget *parent = nullptr);

private slots:
    void onTableSelectionChanged();
    void onAddClicked();
    void onRemoveClicked();
    void onApplyClicked();
    void onPickColorClicked();

private:
    // 搭建表格 + 属性表单 + 底部按钮
    void buildUi();
    // 属性表单（名称/地址/类型/单位/系数/量程/小数位/颜色/启用）
    void buildForm(QVBoxLayout *layout);
    // 按模型重建表格
    void refreshTable();
    // 把指定通道加载到表单
    void loadChannelToForm(int index);
    // 清空表单并退出编辑态
    void clearForm();
    // 读取表单内容；ignoreIndex 用于地址查重时跳过自身
    bool readForm(ChannelItem *item, int ignoreIndex);
    // 按 m_color 刷新颜色按钮
    void updateColorButton();
    // 数据类型枚举 → 显示文本
    QString dataTypeText(ChannelDataType type) const;
    // 返回当前通道中未占用的最小寄存器地址
    quint16 nextFreeAddress() const;

private:
    ChannelModel *m_model = nullptr;
    int m_currentIndex = -1;  // 当前编辑通道下标，-1 表示新增模式
    QColor m_color;           // 表单当前选中的曲线颜色

    QTableWidget *m_table = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QSpinBox *m_addressSpin = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_unitEdit = nullptr;
    QDoubleSpinBox *m_scaleSpin = nullptr;
    QDoubleSpinBox *m_rangeMinSpin = nullptr;
    QDoubleSpinBox *m_rangeMaxSpin = nullptr;
    QSpinBox *m_decimalsSpin = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QPushButton *m_colorButton = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

#endif // CHANNELEDITDIALOG_H
