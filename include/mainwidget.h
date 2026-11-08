#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include "BaseSystemTray.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

// 主窗口,继承自QWidget
class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

    // 初始化主窗口
    void init();
    // 初始化用户界面
    void initUI();
    // 初始化系统托盘图标
    void initSystemTray();

    //初始化信号
    void initSignals();


protected:
    //重新绘图事件，实现绘图
    void paintEvent(QPaintEvent *event) override;
    //重新实现事件处理函数，处理语言变化
    void changeEvent(QEvent *event) override;

public slots:
    //系统托盘点击事件，通过枚举控制
    void onTrayIconClicked(TrayTriggers trigger);

private slots:
    void onUpdateSignal();

private:
    Ui::MainWidget *ui;

    // 系统托盘图标
    BaseSystemTray *systemTray;

};
#endif // MAINWIDGET_H
