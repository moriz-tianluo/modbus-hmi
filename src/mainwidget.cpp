#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include "Language.h"
#include "Signals.h"

MainWidget::MainWidget(BaseWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::MainWidget)
    , systemTray(nullptr)
{
    ui->setupUi(this);
    init();
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::languageChange()
{

}

// 初始化主窗口
void MainWidget::init()
{
    initUI();
    initSystemTray();
    initSignals();
}

// 初始化用户界面
void MainWidget::initUI()
{
    // 自定义 QWidget 子类需开启该属性，样式表中的 background-color/border 才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(LANGUAGE("LG_appVersion"));
    setWindowIcon(QIcon(":/icon/main/appIcon.png"));
}

// 初始化系统托盘图标
void MainWidget::initSystemTray()
{
    systemTray = new BaseSystemTray(this);
    connect(systemTray, &BaseSystemTray::trayIconClicked, this, &MainWidget::onTrayIconClicked);
    qCritical() << "initSystemTray";

}



void MainWidget::initSignals()
{
    connect(SignalsInstance, &Signals::updateSignal, this, &MainWidget::onUpdateSignal);
}


// 重新绘图事件，实现绘图
void MainWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

// 重新实现事件处理函数，处理语言变化
void MainWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        //获取当前语言类型
        LanguageType languageType = Language::getInstance()->convertToLanguageType(QLocale::system().language());
        Language::getInstance()->setLanguageType(languageType);
    }
    else
    {
        QWidget::changeEvent(event);
    }
}



// 系统托盘点击事件，通过枚举控制
void MainWidget::onTrayIconClicked(TrayTriggers trigger)
{
    switch (trigger) {
    case ShowTrayIcon:
        systemTray->showTrayIcon();
        break;
    case HideTrayIcon:
        systemTray->hideTrayIcon();
        break;
    case ExitApplication:
        QApplication::exit(0);
        break;
    case ShowConfigWidget:
    {
        break;
    }
    default:
        break;
    }
}


void MainWidget::onUpdateSignal()
{
    update();
}
