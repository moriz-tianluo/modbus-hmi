#ifndef MENUWIDGET_H
#define MENUWIDGET_H

#include "base/BaseWidget.h"

#include <QStringList>

class QAction;
class QMenu;

namespace Ui {
class menuWidget;
}

// 顶部菜单栏：文件 / 编辑 / 设备 / 视图 / 工具 / 帮助
// 只保留当前已有实现的动作，不放灰色占位项；
// 本类不直接操作业务对象，全部通过信号交给 MainWidget 汇总处理。
class menuWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit menuWidget(QWidget *parent = nullptr);
    ~menuWidget();

    void languageChange() override;

    // 按运行状态刷新动作可用性：未连接时不能断开/采集，采集中不能换端口
    void updateActionStates(bool connected, bool polling);

    // 刷新「最近工程」子菜单内容（最多 5 条，最新在前）
    void setRecentProjects(const QStringList &paths);

    // 同步语言菜单的勾选状态：-1 跟随系统，0 中文，1 英文
    void setLanguageType(int languageType);

signals:
    // ---- 文件 ----
    void exportCsvTriggered();
    void screenshotTriggered();
    void exitTriggered();
    // 工程文件：新建 / 打开 / 保存 / 打开最近工程
    void newProjectTriggered();
    void openProjectTriggered();
    void saveProjectTriggered();
    void recentProjectTriggered(const QString &path);

    // ---- 编辑 ----
    void selectAllChannelsTriggered();
    void unselectAllChannelsTriggered();

    // ---- 设备 ----
    void refreshPortTriggered();
    void openPortTriggered();
    void closePortTriggered();
    void startCollectTriggered();
    void stopCollectTriggered();
    // 通道管理 / 报警规则
    void channelManagerTriggered();
    void alarmRuleTriggered();
    // 写寄存器 / 线圈（05/06/15/16）
    void writeRegisterTriggered();
    // 报警记录 / 数据库维护
    void alarmHistoryTriggered();
    void databaseBackupTriggered();
    void databaseVacuumTriggered();
    // 曲线导出
    void curveScreenshotTriggered();
    void curveExportTriggered();

    // ---- 视图（勾选状态即显示状态）----
    void leftPanelToggled(bool visible);
    void realCurveToggled(bool visible);
    void realDataToggled(bool visible);
    void rightPanelToggled(bool visible);
    // 曲线外观设置（线宽 / 上限参考线）
    void curveSettingsTriggered();

    // ---- 工具 / 帮助 ----
    void openDataDirTriggered();
    void commLogTriggered();
    // 采样数据自动清理天数设置
    void retentionTriggered();
    void aboutTriggered();
    // 快速上手说明
    void quickStartTriggered();
    // 界面语言切换：-1 跟随系统，0 中文，1 英文
    void languageTriggered(int languageType);

private:
    // 创建 6 个菜单并挂到顶部按钮上
    void initMenus();
    void initFileMenu();
    void initEditMenu();
    void initDeviceMenu();
    void initViewMenu();
    void initToolsMenu();
    void initHelpMenu();

    // 语言切换时刷新菜单标题与动作文字（不重建动作，保留勾选状态）
    void retranslateMenus();
    // 最近工程动作统一入口：从发送者携带的路径发起打开请求
    void onRecentActionTriggered();
    // 语言动作统一入口：从发送者携带的语言编号发起切换请求
    void onLanguageActionTriggered();

private:
    Ui::menuWidget *ui;

    QMenu *m_fileMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_deviceMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_toolsMenu = nullptr;
    QMenu *m_helpMenu = nullptr;

    QAction *m_exportCsvAction = nullptr;
    QAction *m_screenshotAction = nullptr;
    QAction *m_exitAction = nullptr;

    QAction *m_newProjectAction = nullptr;
    QAction *m_openProjectAction = nullptr;
    QAction *m_saveProjectAction = nullptr;
    QMenu *m_recentMenu = nullptr;
    QList<QAction *> m_recentActions;  // 最近工程动态动作（复用，避免反复创建）

    QAction *m_selectAllAction = nullptr;
    QAction *m_unselectAllAction = nullptr;

    QAction *m_refreshPortAction = nullptr;
    QAction *m_openPortAction = nullptr;
    QAction *m_closePortAction = nullptr;
    QAction *m_startCollectAction = nullptr;
    QAction *m_stopCollectAction = nullptr;
    QAction *m_channelManagerAction = nullptr;
    QAction *m_alarmRuleAction = nullptr;
    QAction *m_writeRegisterAction = nullptr;
    QAction *m_alarmHistoryAction = nullptr;
    QAction *m_databaseBackupAction = nullptr;
    QAction *m_databaseVacuumAction = nullptr;
    QAction *m_curveScreenshotAction = nullptr;
    QAction *m_curveExportAction = nullptr;

    QAction *m_showLeftAction = nullptr;
    QAction *m_showCurveAction = nullptr;
    QAction *m_showDataAction = nullptr;
    QAction *m_showRightAction = nullptr;
    QAction *m_curveSettingsAction = nullptr;

    QAction *m_openDataDirAction = nullptr;
    QAction *m_commLogAction = nullptr;
    QAction *m_retentionAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_quickStartAction = nullptr;

    QMenu *m_languageMenu = nullptr;
    QAction *m_languageAutoAction = nullptr;
    QAction *m_languageZhAction = nullptr;
    QAction *m_languageEnAction = nullptr;
};

#endif // MENUWIDGET_H
