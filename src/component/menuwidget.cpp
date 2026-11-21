#include "menuwidget.h"
#include "ui_menuwidget.h"
#include "Language.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>

menuWidget::menuWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::menuWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景才会生效
    setAttribute(Qt::WA_StyledBackground, true);

    initMenus();
    languageChange();
}

menuWidget::~menuWidget()
{
    delete ui;
}

void menuWidget::languageChange()
{
    ui->toolButtonFile->setText(LANGUAGE("LG_menu_file"));
    ui->toolButtonEdit->setText(LANGUAGE("LG_menu_edit"));
    ui->toolButtonDevice->setText(LANGUAGE("LG_menu_device"));
    ui->toolButtonView->setText(LANGUAGE("LG_menu_view"));
    ui->toolButtonTool->setText(LANGUAGE("LG_menu_tools"));
    ui->toolButtonHelp->setText(LANGUAGE("LG_menu_help"));

    retranslateMenus();
}

void menuWidget::initMenus()
{
    initFileMenu();
    initEditMenu();
    initDeviceMenu();
    initViewMenu();
    initToolsMenu();
    initHelpMenu();

    // 菜单挂到顶部按钮上，点击按钮即弹出菜单
    ui->toolButtonFile->setMenu(m_fileMenu);
    ui->toolButtonEdit->setMenu(m_editMenu);
    ui->toolButtonDevice->setMenu(m_deviceMenu);
    ui->toolButtonView->setMenu(m_viewMenu);
    ui->toolButtonTool->setMenu(m_toolsMenu);
    ui->toolButtonHelp->setMenu(m_helpMenu);

    ui->toolButtonFile->setPopupMode(QToolButton::InstantPopup);
    ui->toolButtonEdit->setPopupMode(QToolButton::InstantPopup);
    ui->toolButtonDevice->setPopupMode(QToolButton::InstantPopup);
    ui->toolButtonView->setPopupMode(QToolButton::InstantPopup);
    ui->toolButtonTool->setPopupMode(QToolButton::InstantPopup);
    ui->toolButtonHelp->setPopupMode(QToolButton::InstantPopup);
}

void menuWidget::initFileMenu()
{
    m_fileMenu = new QMenu(this);

    m_newProjectAction = m_fileMenu->addAction(QString());
    m_newProjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    connect(m_newProjectAction, &QAction::triggered,
            this, &menuWidget::newProjectTriggered);

    m_openProjectAction = m_fileMenu->addAction(QString());
    m_openProjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    connect(m_openProjectAction, &QAction::triggered,
            this, &menuWidget::openProjectTriggered);

    m_saveProjectAction = m_fileMenu->addAction(QString());
    m_saveProjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    connect(m_saveProjectAction, &QAction::triggered,
            this, &menuWidget::saveProjectTriggered);

    // 最近工程：内容由 MainWidget 通过 setRecentProjects 刷新
    m_recentMenu = m_fileMenu->addMenu(QString());

    m_fileMenu->addSeparator();

    m_exportCsvAction = m_fileMenu->addAction(QString());
    m_exportCsvAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(m_exportCsvAction, &QAction::triggered, this, &menuWidget::exportCsvTriggered);

    m_screenshotAction = m_fileMenu->addAction(QString());
    connect(m_screenshotAction, &QAction::triggered, this, &menuWidget::screenshotTriggered);

    m_fileMenu->addSeparator();

    m_exitAction = m_fileMenu->addAction(QString());
    m_exitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(m_exitAction, &QAction::triggered, this, &menuWidget::exitTriggered);
}

void menuWidget::initEditMenu()
{
    m_editMenu = new QMenu(this);

    m_selectAllAction = m_editMenu->addAction(QString());
    m_selectAllAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+A")));
    m_selectAllAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_selectAllAction, &QAction::triggered, this, &menuWidget::selectAllChannelsTriggered);

    m_unselectAllAction = m_editMenu->addAction(QString());
    connect(m_unselectAllAction, &QAction::triggered, this, &menuWidget::unselectAllChannelsTriggered);
}

void menuWidget::initDeviceMenu()
{
    m_deviceMenu = new QMenu(this);

    m_refreshPortAction = m_deviceMenu->addAction(QString());
    m_refreshPortAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(m_refreshPortAction, &QAction::triggered, this, &menuWidget::refreshPortTriggered);

    m_openPortAction = m_deviceMenu->addAction(QString());
    m_openPortAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    connect(m_openPortAction, &QAction::triggered, this, &menuWidget::openPortTriggered);

    m_closePortAction = m_deviceMenu->addAction(QString());
    m_closePortAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(m_closePortAction, &QAction::triggered, this, &menuWidget::closePortTriggered);

    m_deviceMenu->addSeparator();

    m_startCollectAction = m_deviceMenu->addAction(QString());
    m_startCollectAction->setShortcut(QKeySequence(QStringLiteral("F9")));
    connect(m_startCollectAction, &QAction::triggered, this, &menuWidget::startCollectTriggered);

    // F10 在 Windows 上是系统菜单键，必须用应用级上下文才能抢到
    m_stopCollectAction = m_deviceMenu->addAction(QString());
    m_stopCollectAction->setShortcut(QKeySequence(QStringLiteral("Shift+F10")));
    m_stopCollectAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_stopCollectAction, &QAction::triggered, this, &menuWidget::stopCollectTriggered);

    m_deviceMenu->addSeparator();

    m_channelManagerAction = m_deviceMenu->addAction(QString());
    connect(m_channelManagerAction, &QAction::triggered,
            this, &menuWidget::channelManagerTriggered);

    m_alarmRuleAction = m_deviceMenu->addAction(QString());
    connect(m_alarmRuleAction, &QAction::triggered, this, &menuWidget::alarmRuleTriggered);

    m_deviceMenu->addSeparator();

    m_writeRegisterAction = m_deviceMenu->addAction(QString());
    connect(m_writeRegisterAction, &QAction::triggered,
            this, &menuWidget::writeRegisterTriggered);
}

void menuWidget::initViewMenu()
{
    m_viewMenu = new QMenu(this);

    m_showLeftAction = m_viewMenu->addAction(QString());
    m_showLeftAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    m_showLeftAction->setCheckable(true);
    m_showLeftAction->setChecked(true);
    connect(m_showLeftAction, &QAction::toggled, this, &menuWidget::leftPanelToggled);

    m_showCurveAction = m_viewMenu->addAction(QString());
    m_showCurveAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
    m_showCurveAction->setCheckable(true);
    m_showCurveAction->setChecked(true);
    connect(m_showCurveAction, &QAction::toggled, this, &menuWidget::realCurveToggled);

    m_showDataAction = m_viewMenu->addAction(QString());
    m_showDataAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+3")));
    m_showDataAction->setCheckable(true);
    m_showDataAction->setChecked(true);
    connect(m_showDataAction, &QAction::toggled, this, &menuWidget::realDataToggled);

    m_showRightAction = m_viewMenu->addAction(QString());
    m_showRightAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+4")));
    m_showRightAction->setCheckable(true);
    m_showRightAction->setChecked(true);
    connect(m_showRightAction, &QAction::toggled, this, &menuWidget::rightPanelToggled);

    m_viewMenu->addSeparator();

    m_curveSettingsAction = m_viewMenu->addAction(QString());
    connect(m_curveSettingsAction, &QAction::triggered,
            this, &menuWidget::curveSettingsTriggered);
}

void menuWidget::initToolsMenu()
{
    m_toolsMenu = new QMenu(this);

    m_openDataDirAction = m_toolsMenu->addAction(QString());
    connect(m_openDataDirAction, &QAction::triggered, this, &menuWidget::openDataDirTriggered);

    m_toolsMenu->addSeparator();

    m_commLogAction = m_toolsMenu->addAction(QString());
    connect(m_commLogAction, &QAction::triggered, this, &menuWidget::commLogTriggered);

    m_retentionAction = m_toolsMenu->addAction(QString());
    connect(m_retentionAction, &QAction::triggered, this, &menuWidget::retentionTriggered);

    m_toolsMenu->addSeparator();

    m_alarmHistoryAction = m_toolsMenu->addAction(QString());
    connect(m_alarmHistoryAction, &QAction::triggered,
            this, &menuWidget::alarmHistoryTriggered);

    m_databaseBackupAction = m_toolsMenu->addAction(QString());
    connect(m_databaseBackupAction, &QAction::triggered,
            this, &menuWidget::databaseBackupTriggered);

    m_databaseVacuumAction = m_toolsMenu->addAction(QString());
    connect(m_databaseVacuumAction, &QAction::triggered,
            this, &menuWidget::databaseVacuumTriggered);

    m_toolsMenu->addSeparator();

    m_curveScreenshotAction = m_toolsMenu->addAction(QString());
    connect(m_curveScreenshotAction, &QAction::triggered,
            this, &menuWidget::curveScreenshotTriggered);

    m_curveExportAction = m_toolsMenu->addAction(QString());
    connect(m_curveExportAction, &QAction::triggered,
            this, &menuWidget::curveExportTriggered);
}

void menuWidget::initHelpMenu()
{
    m_helpMenu = new QMenu(this);

    m_quickStartAction = m_helpMenu->addAction(QString());
    connect(m_quickStartAction, &QAction::triggered, this, &menuWidget::quickStartTriggered);

    m_helpMenu->addSeparator();

    // 语言子菜单：跟随系统 / 中文 / 英文，互斥勾选
    m_languageMenu = m_helpMenu->addMenu(QString());
    QActionGroup *languageGroup = new QActionGroup(m_languageMenu);
    languageGroup->setExclusive(true);

    m_languageAutoAction = m_languageMenu->addAction(QString());
    m_languageAutoAction->setCheckable(true);
    m_languageAutoAction->setData(-1);
    languageGroup->addAction(m_languageAutoAction);

    m_languageZhAction = m_languageMenu->addAction(QString());
    m_languageZhAction->setCheckable(true);
    m_languageZhAction->setData(0);
    languageGroup->addAction(m_languageZhAction);

    m_languageEnAction = m_languageMenu->addAction(QString());
    m_languageEnAction->setCheckable(true);
    m_languageEnAction->setData(1);
    languageGroup->addAction(m_languageEnAction);

    connect(m_languageAutoAction, &QAction::triggered,
            this, &menuWidget::onLanguageActionTriggered);
    connect(m_languageZhAction, &QAction::triggered,
            this, &menuWidget::onLanguageActionTriggered);
    connect(m_languageEnAction, &QAction::triggered,
            this, &menuWidget::onLanguageActionTriggered);
    m_languageAutoAction->setChecked(true);

    m_helpMenu->addSeparator();

    m_aboutAction = m_helpMenu->addAction(QString());
    connect(m_aboutAction, &QAction::triggered, this, &menuWidget::aboutTriggered);
}

void menuWidget::setLanguageType(int languageType)
{
    QAction *target = m_languageAutoAction;
    if (languageType == 0) {
        target = m_languageZhAction;
    } else if (languageType == 1) {
        target = m_languageEnAction;
    }

    if (target != nullptr) {
        target->setChecked(true);
    }
}

void menuWidget::onLanguageActionTriggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action == nullptr) {
        return;
    }
    emit languageTriggered(action->data().toInt());
}

void menuWidget::setRecentProjects(const QStringList &paths)
{
    if (m_recentMenu == nullptr) {
        return;
    }

    m_recentMenu->clear();
    m_recentActions.clear();

    if (paths.isEmpty()) {
        QAction *emptyAction = m_recentMenu->addAction(LANGUAGE("LG_menu_recentEmpty"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString &path : paths) {
        QAction *action = m_recentMenu->addAction(path);
        action->setData(path);
        m_recentActions.append(action);
        connect(action, &QAction::triggered,
                this, &menuWidget::onRecentActionTriggered);
    }
}

void menuWidget::onRecentActionTriggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action == nullptr) {
        return;
    }
    emit recentProjectTriggered(action->data().toString());
}

void menuWidget::retranslateMenus()
{
    m_fileMenu->setTitle(LANGUAGE("LG_menu_file"));
    m_editMenu->setTitle(LANGUAGE("LG_menu_edit"));
    m_deviceMenu->setTitle(LANGUAGE("LG_menu_device"));
    m_viewMenu->setTitle(LANGUAGE("LG_menu_view"));
    m_toolsMenu->setTitle(LANGUAGE("LG_menu_tools"));
    m_helpMenu->setTitle(LANGUAGE("LG_menu_help"));

    m_newProjectAction->setText(LANGUAGE("LG_menu_action_newProject"));
    m_openProjectAction->setText(LANGUAGE("LG_menu_action_openProject"));
    m_saveProjectAction->setText(LANGUAGE("LG_menu_action_saveProject"));
    m_recentMenu->setTitle(LANGUAGE("LG_menu_recentProjects"));

    m_exportCsvAction->setText(LANGUAGE("LG_menu_action_exportCsv"));
    m_screenshotAction->setText(LANGUAGE("LG_menu_action_screenshot"));
    m_exitAction->setText(LANGUAGE("LG_menu_action_exit"));

    m_selectAllAction->setText(LANGUAGE("LG_menu_action_selectAll"));
    m_unselectAllAction->setText(LANGUAGE("LG_menu_action_unselectAll"));

    m_refreshPortAction->setText(LANGUAGE("LG_menu_action_refreshPort"));
    m_openPortAction->setText(LANGUAGE("LG_menu_action_connect"));
    m_closePortAction->setText(LANGUAGE("LG_menu_action_disconnect"));
    m_startCollectAction->setText(LANGUAGE("LG_menu_action_startCollect"));
    m_stopCollectAction->setText(LANGUAGE("LG_menu_action_stopCollect"));
    m_channelManagerAction->setText(LANGUAGE("LG_menu_action_channelManager"));
    m_alarmRuleAction->setText(LANGUAGE("LG_menu_action_alarmRule"));
    m_writeRegisterAction->setText(LANGUAGE("LG_menu_action_writeRegister"));

    m_showLeftAction->setText(LANGUAGE("LG_menu_action_showLeft"));
    m_showCurveAction->setText(LANGUAGE("LG_menu_action_showCurve"));
    m_showDataAction->setText(LANGUAGE("LG_menu_action_showData"));
    m_showRightAction->setText(LANGUAGE("LG_menu_action_showRight"));
    m_curveSettingsAction->setText(LANGUAGE("LG_menu_action_curveSettings"));

    m_openDataDirAction->setText(LANGUAGE("LG_menu_action_openDataDir"));
    m_commLogAction->setText(LANGUAGE("LG_menu_action_commLog"));
    m_alarmHistoryAction->setText(LANGUAGE("LG_menu_action_alarmHistory"));
    m_databaseBackupAction->setText(LANGUAGE("LG_menu_action_dbBackup"));
    m_databaseVacuumAction->setText(LANGUAGE("LG_menu_action_dbVacuum"));
    m_curveScreenshotAction->setText(LANGUAGE("LG_menu_action_curveScreenshot"));
    m_curveExportAction->setText(LANGUAGE("LG_menu_action_curveExport"));
    m_retentionAction->setText(LANGUAGE("LG_menu_action_retention"));
    m_aboutAction->setText(LANGUAGE("LG_menu_action_about"));
    m_quickStartAction->setText(LANGUAGE("LG_menu_action_quickStart"));

    m_languageMenu->setTitle(LANGUAGE("LG_menu_action_language"));
    m_languageAutoAction->setText(LANGUAGE("LG_language_followSystem"));
    m_languageZhAction->setText(LANGUAGE("LG_language_chinese"));
    m_languageEnAction->setText(LANGUAGE("LG_language_english"));
}

void menuWidget::updateActionStates(bool connected, bool polling)
{
    m_openPortAction->setEnabled(!connected);
    m_closePortAction->setEnabled(connected);
    m_startCollectAction->setEnabled(connected && !polling);
    m_stopCollectAction->setEnabled(polling);
    m_exportCsvAction->setEnabled(connected);
    m_refreshPortAction->setEnabled(!polling);
    // 写操作必须已连接串口
    m_writeRegisterAction->setEnabled(connected);

    // 采集中不允许切换工程文件，避免通道映射与采集参数错位
    m_newProjectAction->setEnabled(!polling);
    m_openProjectAction->setEnabled(!polling);
    m_recentMenu->setEnabled(!polling);
}
