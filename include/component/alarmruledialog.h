#ifndef ALARMRULEDIALOG_H
#define ALARMRULEDIALOG_H

#include <QDialog>

class ChannelModel;
class QSpinBox;
class QTableWidget;

// 报警规则配置对话框：
//   - 每个通道可单独启用越限报警并设置上限/下限（工程值）
//   - 连续越限次数全局配置，达到次数才记录一条报警
// 点击确定后一次性写回 ChannelModel，避免逐通道刷新表格。
class AlarmRuleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlarmRuleDialog(ChannelModel *model, int debounceCount,
                             QWidget *parent = nullptr);

    // 用户确认后的连续越限次数
    int debounceCount() const;

signals:
    // 连续越限次数变化（仅在点击确定后发射）
    void debounceCountChanged(int count);

private slots:
    void onAcceptClicked();

private:
    // 搭建表格、次数设置与按钮
    void buildUi();
    // 按模型填充通道行（启用勾选 + 上下限输入框）
    void populateTable();
    // 把表格内容写回模型
    bool applyRules();

    ChannelModel *m_model = nullptr;
    int m_debounceCount = 3;

    QTableWidget *m_table = nullptr;
    QSpinBox *m_debounceSpin = nullptr;
};

#endif // ALARMRULEDIALOG_H
