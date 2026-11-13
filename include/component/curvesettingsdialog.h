#ifndef CURVESETTINGSDIALOG_H
#define CURVESETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

// 曲线外观设置对话框：线宽、上限参考线显示开关与数值。
// 只负责收集用户输入，应用与持久化由 MainWidget 完成。
class CurveSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CurveSettingsDialog(int lineWidth, bool limitVisible, double limitValue,
                                 QWidget *parent = nullptr);

    int lineWidth() const;
    bool upperLimitVisible() const;
    double upperLimitValue() const;

private:
    // 搭建表单与底部按钮
    void buildUi();

private:
    QSpinBox *m_widthSpin = nullptr;
    QCheckBox *m_limitCheck = nullptr;
    QDoubleSpinBox *m_limitSpin = nullptr;
};

#endif // CURVESETTINGSDIALOG_H
