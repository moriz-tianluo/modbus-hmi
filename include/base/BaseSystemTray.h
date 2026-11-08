#ifndef BASESYSTEMTRAY_H
#define BASESYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QIcon>

// 系统托盘图标点击事件
enum TrayTriggers {
    ShowTrayIcon = 0,//显示系统托盘图标
    HideTrayIcon,//隐藏系统托盘图标
    ExitApplication,//退出应用
    ShowConfigWidget,//配置界面
};


// 系统托盘图标
class BaseSystemTray : public QObject
{
    Q_OBJECT
public:
    explicit BaseSystemTray(QObject *parent = nullptr);
    ~BaseSystemTray();

    void init();

    // 初始化系统托盘图标菜单
    void initMenu();
    // 创建系统托盘图标
    void createTrayIcon();
    // 初始化系统托盘图标
    void initTrayIcon();

    // 显示系统托盘图标
    void showTrayIcon();
    void hideTrayIcon();

signals:
    void trayIconClicked(TrayTriggers trigger);

public slots:
    void exitApplication();
    // 显示配置界面
    void showConfigWidget();

private:
    QSystemTrayIcon *trayIcon;
    QMenu *menu;
};




#endif // BASESYSTEMTRAY_H
