#include "BaseSystemTray.h"
#include <QDebug>
#include "Language.h"

BaseSystemTray::BaseSystemTray(QObject *parent)
    : QObject(parent)
    , trayIcon(nullptr)
    , menu(nullptr)
{
    init();
}

BaseSystemTray::~BaseSystemTray()
{
    delete menu;
}

void BaseSystemTray::init()
{
    initTrayIcon();
}

void BaseSystemTray::initMenu()
{
    menu = new QMenu();

    // 显示配置界面
    QAction *configAction = new QAction(LANGUAGE_INSTANCE->getLanguageString("LG_config"), this);
    connect(configAction, &QAction::triggered, this, &BaseSystemTray::showConfigWidget);
    menu->addAction(configAction);


    // 退出应用
    QAction *exitAction = new QAction(LANGUAGE_INSTANCE->getLanguageString("LG_quit"), this);
    connect(exitAction, &QAction::triggered, this, &BaseSystemTray::exitApplication);
    menu->addAction(exitAction);
}

void BaseSystemTray::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);

    // 尝试使用 PNG 格式图标（更可靠）
    QIcon icon(":/icon/main/appIcon.png");
    
    // 检查图标是否加载成功
    if (icon.isNull()) {
        qWarning() << "Failed to load tray icon: :/icon/main/Sight.png";
        // 如果 PNG 也失败，尝试使用系统默认图标
        icon = QIcon::fromTheme("application-x-executable");
    }
    
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("FrontSight");

    qInfo() << "createTrayIcon - icon loaded:" << !icon.isNull();
}

void BaseSystemTray::initTrayIcon()
{
    initMenu();
    createTrayIcon();
    trayIcon->setContextMenu(menu);

    // 检查系统托盘是否可用
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon->show();
        qInfo() << "System tray icon displayed";
    } else {
        qWarning() << "System tray is not available on this system";
    }
}


void BaseSystemTray::showTrayIcon()
{
    trayIcon->show();
}

void BaseSystemTray::hideTrayIcon()
{
    trayIcon->hide();
}

void BaseSystemTray::showMessage(const QString &title, const QString &text, int timeoutMs)
{
    if (trayIcon == nullptr || !trayIcon->isVisible()) {
        return;
    }
    trayIcon->showMessage(title, text, QSystemTrayIcon::Warning, timeoutMs);
}

void BaseSystemTray::exitApplication()
{
    emit trayIconClicked(ExitApplication);
}

void BaseSystemTray::showConfigWidget()
{
    emit trayIconClicked(ShowConfigWidget);
}


