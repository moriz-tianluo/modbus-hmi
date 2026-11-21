#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QJsonObject>
#include <QWidget>
#include "BaseWidget.h"
#include "BaseSystemTray.h"

// 核心头文件 include（信号槽参数类型需要精确匹配）
#include "common/projectconfig.h"
#include "common/serialworker.h"
#include "common/channelmodel.h"
#include "common/savedatasql.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

class AlarmRuleDialog;
class ChannelEditDialog;
class CommLogDialog;
class HistoryQueryDialog;
class QTimer;

// 主窗口 —— 同时作为系统核心中转枢纽：
//   UI 控件（配置面板/操作工具条）→ MainWidget（汇总校验）→ SerialWorker（工作线程）
//   SerialWorker（数据/状态）→ MainWidget → ChannelModel（数据更新）→ UI 显示控件
// 核心对象所有权：SerialWorker 在 QThread 内运行（moveToThread），ChannelModel 在 UI 线程
class MainWidget : public BaseWidget
{
    Q_OBJECT

public:
    MainWidget(BaseWidget *parent = nullptr);
    ~MainWidget();

    void languageChange() override;

    // 初始化主窗口（UI + 托盘 + 信号 + 核心 worker + channel model）
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
    // 关闭前把界面配置持久化到 project.ini
    void closeEvent(QCloseEvent *event) override;

public slots:
    //系统托盘点击事件，通过枚举控制
    void onTrayIconClicked(TrayTriggers trigger);

private slots:
    void onUpdateSignal();

    // ---- UI 请求下发（operationWidget 按钮信号）----
    void onOpenPortRequested();
    void onClosePortRequested();
    void onStartCollectRequested();
    void onStopCollectRequested();

    // ---- 顶部工具条按钮 ----
    void onExportCsvClicked();
    void onScreenshotClicked();

    // ---- 顶部菜单动作 ----
    void onSelectAllChannelsTriggered();
    void onUnselectAllChannelsTriggered();
    void onOpenDataDirTriggered();
    void onAboutTriggered();

    // ---- 视图显隐（菜单勾选项）----
    void onLeftPanelToggled(bool visible);
    void onRealCurveToggled(bool visible);
    void onRealDataToggled(bool visible);
    void onRightPanelToggled(bool visible);

    // ---- 通道卡片/曲线卡片 ----
    void onSaveToDatabaseToggled(bool enabled);
    void onCurvePauseToggled(bool paused);
    void onCurvePauseStateChanged(bool paused);
    // 通道配置导入/导出成功 → 记忆 JSON 路径
    void onChannelConfigPathChanged(const QString &path);

    // ---- 通道管理 / 报警规则 / 通信日志 ----
    void onChannelManagerTriggered();
    void onAlarmRuleTriggered();
    void onCommLogTriggered();
    // 写寄存器 / 线圈
    void onWriteRegisterTriggered();
    void onWriteRegistersRequested(quint8 slaveId, quint16 address,
                                   const QVector<quint16> &values);
    void onWriteSingleCoilRequested(quint8 slaveId, quint16 address, bool on);
    void onWriteCoilsRequested(quint8 slaveId, quint16 address, const QVector<bool> &values);
    // 报警记录 / 数据库维护
    void onAlarmHistoryTriggered();
    void onDatabaseBackupTriggered();
    void onDatabaseVacuumTriggered();
    void onDatabaseMaintenanceFinished(bool ok, const QString &action,
                                       const QString &errorString);
    // 曲线导出
    void onCurveScreenshotTriggered();
    void onCurveExportTriggered();
    // 报警提醒：声音 + 托盘气泡
    void onAlarmRecordAdded(int level, const QString &message);
    // 报警规则对话框确认后更新去抖次数
    void onAlarmDebounceChanged(int count);
    // 通道增删改后重置越限计数状态
    void onChannelListChanged();

    // ---- 历史查询 ----
    void onHistoryRequested(int channelIndex);
    void onHistoryQueryRequestedFromCard(qint64 fromMs, qint64 toMs);
    void onHistoryQueryRequested(qint64 fromMs, qint64 toMs, int channelIndex);

    // ---- 工程文件 ----
    void onNewProjectTriggered();
    void onOpenProjectTriggered();
    void onSaveProjectTriggered();
    void onRecentProjectTriggered(const QString &path);

    // ---- 曲线设置 / 数据保留 / 快速上手 ----
    void onCurveSettingsTriggered();
    void onRetentionTriggered();
    void onQuickStartTriggered();
    // 定时把数据库总记录数刷新到实时数据卡片脚注
    void onDbTipTimerTimeout();
    // 界面语言切换（来自帮助 → 语言菜单）
    void onLanguageTriggered(int languageType);

    // ---- SerialWorker 状态上收 ----
    void onWorkerPortOpened(const QString &portName);
    void onWorkerPortClosed(const QString &portName);
    void onWorkerPortError(QSerialPort::SerialPortError error, const QString &message);
    void onWorkerReconnectStateChanged(bool reconnecting, int attempt);
    void onWorkerPollingStateChanged(bool running);
    void onWorkerPollFailed(ModbusError error, const QString &message);
    void onWorkerDataUpdated(quint8 slaveId, quint16 startAddress,
                             const QVector<quint16> &values, qint64 rttMs);
    // 写操作结果
    void onWorkerWriteSucceeded(quint8 functionCode, quint16 startAddress, quint16 quantity);
    void onWorkerWriteFailed(ModbusError error, const QString &message);
    // 数据库写入出错（只更新状态栏，避免再次触发报警造成递归）
    void onDatabaseError(const QString &message);

private:
    // 创建并启动工作线程：先 ChannelModel（UI），再 SerialWorker + QThread
    void initWorker();
    // 创建数据库线程：SaveDataSQL + QThread，并打开 data/hmi.db
    void initDatabase();
    // 通道表为空时注入 4 个演示通道（地址 0~3，对齐 python/slave_sim.py）
    void seedDemoChannels();
    // 把 MainWidget 的汇总槽连到 SerialWorker 信号（跨线程自动队列化）
    void initWorkerConnections();
    // 把 ChannelModel 信号连到显示控件（表格/曲线）
    void initChannelConnections();
    // 菜单/工具条按钮 → MainWidget 槽
    void initMenuConnections();
    // 创建并加载 ProjectConfig（串口/Modbus/界面偏好）
    void initProjectConfig();
    // 用已加载的配置回填界面（需 ChannelModel 已创建）
    void restoreProjectConfig();
    // 把界面当前配置写回 ProjectConfig 并落盘
    void persistProjectConfig();
    // 保存界面截图（带文件对话框）
    void saveScreenshot();
    // 按当前连接/采集状态刷新菜单动作可用性
    void refreshActionStates();

    // ---- 界面语言 ----
    // 应用语言：languageType -1 跟随系统 / 0 中文 / 1 英文；persist=true 时写入配置
    void applyLanguage(int languageType, bool persist);

    // ---- 工程脏标记 ----
    // 监听会改变工程内容的信号，统一标记「未保存」
    void initProjectDirtyTracking();
    // 标记工程已修改并刷新标题栏
    void markProjectDirty();
    // 清除未保存标记并刷新标题栏
    void clearProjectDirty();
    // 有未保存修改时询问保存；返回 false 表示用户取消当前操作
    bool maybeSaveProject();
    // 保存到当前工程文件（不存在时弹文件对话框），成功返回 true
    bool saveProjectInteractive();

    // ---- 报警阈值判定 ----
    // 每轮采集后检查各通道工程值，连续越限达到去抖次数才记录报警，恢复后记录解除
    void evaluateAlarmThresholds();
    // 单通道越限报警文本（区分超上限/低于下限）
    void raiseChannelAlarm(const ChannelItem &item, bool overHigh, double value);
    // 工程值 + 单位文本，用于报警消息
    QString alarmValueText(const ChannelItem &item, double value) const;
    // 刷新报警卡片底部规则提示
    void updateAlarmParamTip();

    // ---- 对话框懒创建 ----
    void ensureHistoryDialog();
    void ensureCommLogDialog();

    // ---- 工程文件读写 ----
    // 从界面控件收集串口配置
    ProjectConfig::SerialSettings currentSerialSettings() const;
    // 从界面控件收集 Modbus 采集参数
    ProjectConfig::ModbusSettings currentModbusSettings() const;
    // 把当前工程组装为 JSON（串口 + Modbus + 通道 + 报警规则）
    QJsonObject currentProjectJson() const;
    // 应用工程 JSON 到界面与数据模型，失败时写 errorMessage
    bool applyProjectJson(const QJsonObject &root, QString *errorMessage);
    // 写入工程文件并记录最近文件
    bool saveProjectToFile(const QString &path);
    // 读取工程文件并应用
    bool loadProjectFromFile(const QString &path);
    // 刷新「最近工程」子菜单
    void refreshRecentProjects();
    // 标题栏追加当前工程文件名
    void updateWindowTitle();
    // 应用曲线外观参数到控件与配置
    void applyCurveSettings(int lineWidth, bool limitVisible, double limitValue);

private:
    // 单通道报警运行状态（连续越限计数 + 是否已处于报警中）
    struct ChannelAlarmState {
        int overCount = 0;   // 连续越限轮数
        bool active = false; // 是否已记录报警（未恢复前不重复记录）
    };

    Ui::MainWidget *ui;

    // 系统托盘图标
    BaseSystemTray *systemTray;

    // ---- 核心对象 ----
    SerialWorker *m_worker = nullptr;       // 工作线程对象，无 parent
    QThread *m_workerThread = nullptr;      // 工作线程
    ChannelModel *m_channelModel = nullptr; // 通道数据模型（UI 线程）
    SaveDataSQL *m_saveData = nullptr;      // 数据存储对象，无 parent（数据库线程）
    QThread *m_saveDataThread = nullptr;    // 数据库线程
    ProjectConfig *m_projectConfig = nullptr; // 项目配置持久化（UI 线程）

    // ---- 运行状态缓存（用于刷新菜单动作可用性）----
    bool m_portOpen = false;          // 串口是否已打开
    bool m_polling = false;           // 是否正在采集
    bool m_saveToDatabase = true;     // 是否落库（通道卡片开关）

    // ---- 报警规则 ----
    QVector<ChannelAlarmState> m_alarmStates;  // 与通道下标一一对应的运行状态
    int m_alarmDebounceCount = 3;              // 连续越限次数阈值

    // ---- 对话框（懒创建，父对象为 MainWidget）----
    HistoryQueryDialog *m_historyDialog = nullptr;
    CommLogDialog *m_commLogDialog = nullptr;

    // 定时刷新数据库总记录数（实时数据卡片脚注）
    QTimer *m_dbTipTimer = nullptr;

    // ---- 工程未保存状态 ----
    bool m_projectDirty = false;          // 工程内容是否有未保存修改
    bool m_suppressProjectDirty = false;  // 回填/加载工程期间屏蔽脏标记
};

#endif // MAINWIDGET_H
