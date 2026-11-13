#include "mainwidget.h"
#include "ui_mainwidget.h"

#include "Language.h"
#include "Signals.h"

#include "component/operationwidget.h"
#include "component/comsetwidget.h"
#include "component/modbussetwidget.h"
#include "component/channelsetwidget.h"
#include "component/realdatawidget.h"
#include "component/realcurvewidget.h"
#include "component/collectionstatisticswodget.h"
#include "component/alarmandfaultwidget.h"
#include "component/datasavewidget.h"
#include "component/menuwidget.h"

#include "component/alarmruledialog.h"
#include "component/channeleditdialog.h"
#include "component/commlogdialog.h"
#include "component/curvesettingsdialog.h"
#include "component/historyquerydialog.h"

#include "common/projectconfig.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QUrl>

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
    // 兜底落盘：托盘退出等不经过 closeEvent 的场景也能保存配置
    persistProjectConfig();

    // 优雅关闭工作线程：先 stopPolling，再 closePort，最后 quit + wait
    if (m_worker && m_workerThread) {
        QMetaObject::invokeMethod(m_worker, "stopPolling", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_worker, "closePort", Qt::BlockingQueuedConnection);
        m_workerThread->quit();
        m_workerThread->wait();
    }

    // 关闭数据库线程：先落盘剩余采样，再关闭连接（阻塞等待，保证数据不丢）
    if (m_saveData && m_saveDataThread) {
        QMetaObject::invokeMethod(m_saveData, "flush", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_saveData, "closeDatabase", Qt::BlockingQueuedConnection);
        m_saveDataThread->quit();
        m_saveDataThread->wait();
    }

    delete ui;
}

void MainWidget::languageChange()
{
    // 预留：语言变化时通知所有子控件刷新
}

// ============== 初始化主窗口 ==============

void MainWidget::init()
{
    initProjectConfig();
    initUI();
    initSystemTray();
    initSignals();
    initWorker();
    initDatabase();
    initWorkerConnections();
    initChannelConnections();
    initMenuConnections();
    // 配置回填放在所有控件与连接就绪之后，避免回填触发的信号无人接收
    restoreProjectConfig();

    // 曲线外观、最近工程列表与标题栏
    applyCurveSettings(m_projectConfig->curveLineWidth(),
                       m_projectConfig->curveUpperLimitVisible(),
                       m_projectConfig->curveUpperLimit());
    refreshRecentProjects();
    updateWindowTitle();
}

// ============== 项目配置持久化 ==============

void MainWidget::initProjectConfig()
{
    m_projectConfig = new ProjectConfig(this);
    m_projectConfig->load();
}

void MainWidget::restoreProjectConfig()
{
    if (m_projectConfig == nullptr) {
        return;
    }

    // 1. 窗口几何（首次启动时为空，保持默认尺寸）
    const QByteArray geometry = m_projectConfig->windowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    // 2. 串口参数
    const ProjectConfig::SerialSettings serial = m_projectConfig->serialSettings();
    SerialPortConfig serialConfig;
    serialConfig.portName = serial.portName;
    serialConfig.baudRate = serial.baudRate;
    serialConfig.dataBits = static_cast<QSerialPort::DataBits>(serial.dataBits);
    serialConfig.parity = static_cast<QSerialPort::Parity>(serial.parity);
    serialConfig.stopBits = static_cast<QSerialPort::StopBits>(serial.stopBits);
    ui->comSetWidget->applySettings(serialConfig, serial.responseTimeoutMs,
                                    serial.retryCount);

    // 3. Modbus 采集参数
    const ProjectConfig::ModbusSettings modbus = m_projectConfig->modbusSettings();
    ui->modbusSetWidget->applySettings(modbus.functionCodeIndex, modbus.startAddress,
                                       modbus.quantity, modbus.slaveId, modbus.intervalMs);

    // 4. 上次使用的通道配置 JSON（文件仍存在才加载，避免启动报错）
    const QString configPath = m_projectConfig->channelConfigPath();
    if (!configPath.isEmpty() && QFile::exists(configPath) && m_channelModel != nullptr) {
        m_channelModel->loadFromJson(configPath);
    }

    // 5. 落库开关与曲线缓冲点数
    ui->channelSetWidget->setSaveToDatabase(m_projectConfig->saveToDatabase());
    ui->channelSetWidget->setBufferSizeValue(m_projectConfig->bufferSize());
    ui->realCurveWidget->setTimeWindowSeconds(m_projectConfig->curveWindowSeconds());

    // 6. 报警去抖次数
    m_alarmDebounceCount = m_projectConfig->alarmDebounceCount();
    updateAlarmParamTip();
}

void MainWidget::persistProjectConfig()
{
    if (m_projectConfig == nullptr) {
        return;
    }

    // 1. 窗口几何
    m_projectConfig->setWindowGeometry(saveGeometry());

    // 2. 串口参数：从 UI 读回，保证用户改过就记住
    m_projectConfig->setSerialSettings(currentSerialSettings());

    // 3. Modbus 采集参数
    m_projectConfig->setModbusSettings(currentModbusSettings());

    // 4. 界面偏好
    m_projectConfig->setSaveToDatabase(ui->channelSetWidget->saveToDatabase());
    m_projectConfig->setBufferSize(ui->channelSetWidget->bufferSize());
    m_projectConfig->setCurveWindowSeconds(ui->realCurveWidget->timeWindowSeconds());
    m_projectConfig->setAlarmDebounceCount(m_alarmDebounceCount);

    // 5. 通道配置路径由 onChannelConfigPathChanged 实时写入，这里统一落盘
    // 6. 通道配置自动保存：优先上次导入/导出的 JSON 路径，否则写入 data/channels.json
    if (m_channelModel != nullptr && m_channelModel->channelCount() > 0) {
        QString path = m_projectConfig->channelConfigPath();
        if (path.isEmpty()) {
            path = QDir(QCoreApplication::applicationDirPath())
                       .filePath(QStringLiteral("data/channels.json"));
            m_projectConfig->setChannelConfigPath(path);
        }
        QString errorMessage;
        m_channelModel->saveToJson(path, &errorMessage);
    }

    m_projectConfig->save();
}

void MainWidget::onChannelConfigPathChanged(const QString &path)
{
    if (m_projectConfig != nullptr) {
        m_projectConfig->setChannelConfigPath(path);
    }
}

void MainWidget::closeEvent(QCloseEvent *event)
{
    persistProjectConfig();
    BaseWidget::closeEvent(event);
}

// ============== ChannelModel → 显示控件 ==============

void MainWidget::initChannelConnections()
{
    // ---- 数据更新 ----
    connect(m_channelModel, &ChannelModel::channelValueChanged,
            ui->realDataWidget, &RealDataWidget::onChannelValueChanged);
    connect(m_channelModel, &ChannelModel::channelValueChanged,
            ui->realCurveWidget, &RealCurveWidget::onChannelValueChanged);
    connect(m_channelModel, &ChannelModel::channelsChanged,
            ui->channelSetWidget, &ChannelSetWidget::onChannelsChanged);
    // 通道配置变化 → 实时数据/实时曲线同步（名称、单位、通道数量）
    connect(m_channelModel, &ChannelModel::channelsChanged,
            ui->realDataWidget, &RealDataWidget::onChannelsChanged);
    connect(m_channelModel, &ChannelModel::channelsChanged,
            ui->realCurveWidget, &RealCurveWidget::onChannelsChanged);
    // 通道启用开关 → 数据行置灰/状态列、曲线隐藏/图例置灰
    connect(m_channelModel, &ChannelModel::channelEnabledChanged,
            ui->realDataWidget, &RealDataWidget::onChannelEnabledChanged);
    connect(m_channelModel, &ChannelModel::channelEnabledChanged,
            ui->realCurveWidget, &RealCurveWidget::onChannelEnabledChanged);
    // 通道增删改 → 重置报警越限计数，避免旧状态错位
    connect(m_channelModel, &ChannelModel::channelsChanged,
            this, &MainWidget::onChannelListChanged);

    // 实时数据选中行 ↔ 曲线高亮；曲线图例点击 → 反选数据行
    connect(ui->realDataWidget, &RealDataWidget::channelSelected,
            ui->realCurveWidget, &RealCurveWidget::setHighlightedChannel);
    connect(ui->realCurveWidget, &RealCurveWidget::channelSelected,
            ui->realDataWidget, &RealDataWidget::selectChannelRow);

    // 导入/导出通道配置后记忆 JSON 路径，下次启动自动恢复
    connect(ui->channelSetWidget, &ChannelSetWidget::configPathChanged,
            this, &MainWidget::onChannelConfigPathChanged);
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
    connect(SignalsInstance, &Signals::updateSignal,
            this, &MainWidget::onUpdateSignal);
}

// ============== 核心 Worker + ChannelModel 初始化 ==============

void MainWidget::initWorker()
{
    // 1. ChannelModel 在 UI 线程创建（以 this 为 parent，生命周期跟随 MainWidget）
    m_channelModel = new ChannelModel(this);

    // 2. 通道表为空时注入演示通道（先注入，后续 setChannelModel 才能刷出表格）
    if (m_channelModel->channelCount() == 0) {
        seedDemoChannels();
    }

    // 3. 注入 ChannelModel 给 UI 显示控件（绑定表格/曲线数据来源）
    ui->channelSetWidget->setChannelModel(m_channelModel);
    ui->realDataWidget->setChannelModel(m_channelModel);
    ui->realCurveWidget->setChannelModel(m_channelModel);

    // 4. SerialWorker 无 parent，后续 moveToThread
    m_worker = new SerialWorker(nullptr);

    // 5. QThread + moveToThread
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    // 线程退出时自动 deleteLater worker（需先 quit + wait）
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread->start();
}

// ============== 数据库线程初始化 ==============

void MainWidget::initDatabase()
{
    // 1. SaveDataSQL 无 parent，后续整体迁入数据库线程
    m_saveData = new SaveDataSQL(nullptr);

    // 2. QThread + moveToThread（与 SerialWorker 同款模式）
    m_saveDataThread = new QThread(this);
    m_saveData->moveToThread(m_saveDataThread);
    connect(m_saveDataThread, &QThread::finished, m_saveData, &QObject::deleteLater);
    connect(m_saveData, &SaveDataSQL::databaseError,
            this, &MainWidget::onDatabaseError);

    m_saveDataThread->start();

    // 3. 打开程序目录下的 data/hmi.db（目录与表结构由 SaveDataSQL 负责创建）
    const QString dbPath = QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral("data/hmi.db"));
    QMetaObject::invokeMethod(m_saveData, "openDatabase", Qt::QueuedConnection,
                              Q_ARG(QString, dbPath));

    // 4. 定时刷新总记录数，驱动实时数据卡片脚注
    m_dbTipTimer = new QTimer(this);
    m_dbTipTimer->setInterval(5000);
    connect(m_dbTipTimer, &QTimer::timeout,
            this, &MainWidget::onDbTipTimerTimeout);
    m_dbTipTimer->start();

    // 5. 启动时按保留策略清理历史数据（0 天表示不清理）
    const int retentionDays = (m_projectConfig != nullptr)
                                  ? m_projectConfig->dataRetentionDays()
                                  : 0;
    if (retentionDays > 0) {
        QMetaObject::invokeMethod(m_saveData, "purgeOlderThan", Qt::QueuedConnection,
                                  Q_ARG(int, retentionDays));
    }
}

void MainWidget::onDbTipTimerTimeout()
{
    if (m_saveData == nullptr) {
        return;
    }

    // fromMs = 0 表示全量统计，实时数据卡片据此刷新脚注
    QMetaObject::invokeMethod(m_saveData, "queryRecordCount", Qt::QueuedConnection,
                              Q_ARG(qint64, 0),
                              Q_ARG(qint64, QDateTime::currentMSecsSinceEpoch()));
}

// 注入 4 个演示通道：寄存器地址 0~3，与 python/slave_sim.py 的默认信号一一对应
void MainWidget::seedDemoChannels()
{
    QVector<ChannelItem> channels;

    // 0x0000 温度：正弦波动（raw 200~800 → 20~80℃）
    ChannelItem temp;
    temp.name = LANGUAGE("LG_curve_legend_temp");
    temp.unit = LANGUAGE("LG_unit_c");
    temp.registerAddress = 0;
    temp.scale = 0.1;
    temp.rangeMin = 0.0;
    temp.rangeMax = 100.0;
    channels.append(temp);

    // 0x0001 压力：锯齿递增（raw 0~999 → 0~99.9MPa）
    ChannelItem press;
    press.name = LANGUAGE("LG_curve_legend_press");
    press.unit = LANGUAGE("LG_unit_mpa");
    press.registerAddress = 1;
    press.scale = 0.1;
    press.rangeMin = 0.0;
    press.rangeMax = 100.0;
    channels.append(press);

    // 0x0002 流量：随机噪声（raw 0~100 → 0~10m³/h）
    ChannelItem flow;
    flow.name = LANGUAGE("LG_curve_legend_flow");
    flow.unit = LANGUAGE("LG_unit_m3h");
    flow.registerAddress = 2;
    flow.scale = 0.1;
    flow.rangeMin = 0.0;
    flow.rangeMax = 10.0;
    channels.append(flow);

    // 0x0003 液位：常量 1234（→123.4%），量程放大到 200 便于观察钳位
    ChannelItem level;
    level.name = LANGUAGE("LG_curve_legend_level");
    level.unit = LANGUAGE("LG_unit_percent");
    level.registerAddress = 3;
    level.scale = 0.1;
    level.rangeMin = 0.0;
    level.rangeMax = 200.0;
    channels.append(level);

    m_channelModel->setChannels(channels);
}

void MainWidget::initWorkerConnections()
{
    // ---- 控件 → MainWidget（UI 操作请求）----
    connect(ui->OperationWidget, &operationWidget::openComClicked,
            this, &MainWidget::onOpenPortRequested);
    connect(ui->OperationWidget, &operationWidget::closeComClicked,
            this, &MainWidget::onClosePortRequested);
    connect(ui->OperationWidget, &operationWidget::startCollectClicked,
            this, &MainWidget::onStartCollectRequested);
    connect(ui->OperationWidget, &operationWidget::stopCollectClicked,
            this, &MainWidget::onStopCollectRequested);

    // ---- SerialWorker → MainWidget（工作线程状态回流，跨线程自动队列化）----
    connect(m_worker, &SerialWorker::portOpened,
            this, &MainWidget::onWorkerPortOpened);
    connect(m_worker, &SerialWorker::portClosed,
            this, &MainWidget::onWorkerPortClosed);
    connect(m_worker, &SerialWorker::portError,
            this, &MainWidget::onWorkerPortError);
    connect(m_worker, &SerialWorker::reconnectStateChanged,
            this, &MainWidget::onWorkerReconnectStateChanged);
    connect(m_worker, &SerialWorker::pollingStateChanged,
            this, &MainWidget::onWorkerPollingStateChanged);
    connect(m_worker, &SerialWorker::pollFailed,
            this, &MainWidget::onWorkerPollFailed);
    connect(m_worker, &SerialWorker::dataUpdated,
            this, &MainWidget::onWorkerDataUpdated);

    // ---- 报警记录 → 数据库线程（去抖后的新记录才落库）----
    connect(ui->alarmAndfaultWidget, &AlarmAndFaultWidget::alarmRecordAdded,
            m_saveData, &SaveDataSQL::enqueueAlarm, Qt::QueuedConnection);

    // ---- 数据存储卡片 → 数据库线程（查询记录数 / 导出 CSV），结果回流到卡片 ----
    connect(ui->dataSaveWidget, &DataSaveWidget::recordCountRequested,
            m_saveData, &SaveDataSQL::queryRecordCount, Qt::QueuedConnection);
    connect(ui->dataSaveWidget, &DataSaveWidget::exportRequested,
            m_saveData, &SaveDataSQL::exportSamplesToCsv, Qt::QueuedConnection);
    connect(m_saveData, &SaveDataSQL::recordCountReady,
            ui->dataSaveWidget, &DataSaveWidget::onRecordCountReady, Qt::QueuedConnection);
    // 全量记录数同时刷新实时数据卡片脚注（卡片内只接受 fromMs == 0 的结果）
    connect(m_saveData, &SaveDataSQL::recordCountReady,
            ui->realDataWidget, &RealDataWidget::onRecordCountReady, Qt::QueuedConnection);
    connect(m_saveData, &SaveDataSQL::exportFinished,
            ui->dataSaveWidget, &DataSaveWidget::onExportFinished, Qt::QueuedConnection);

    // ---- 历史查询 / 通信日志对话框（依赖 m_worker 与 m_saveData，在此创建并接线）----
    ensureHistoryDialog();
    ensureCommLogDialog();
}

// ============== 顶部菜单 / 工具条按钮接线 ==============

void MainWidget::initMenuConnections()
{
    // ---- 顶部工具条按钮 ----
    connect(ui->OperationWidget, &operationWidget::exportCsvClicked,
            this, &MainWidget::onExportCsvClicked);
    connect(ui->OperationWidget, &operationWidget::screenshotClicked,
            this, &MainWidget::onScreenshotClicked);
    connect(ui->OperationWidget, &operationWidget::curvePauseToggled,
            this, &MainWidget::onCurvePauseToggled);

    // ---- 文件 ----
    connect(ui->MenuWidget, &menuWidget::exportCsvTriggered,
            this, &MainWidget::onExportCsvClicked);
    connect(ui->MenuWidget, &menuWidget::screenshotTriggered,
            this, &MainWidget::onScreenshotClicked);
    connect(ui->MenuWidget, &menuWidget::exitTriggered,
            this, &MainWidget::close);
    connect(ui->MenuWidget, &menuWidget::newProjectTriggered,
            this, &MainWidget::onNewProjectTriggered);
    connect(ui->MenuWidget, &menuWidget::openProjectTriggered,
            this, &MainWidget::onOpenProjectTriggered);
    connect(ui->MenuWidget, &menuWidget::saveProjectTriggered,
            this, &MainWidget::onSaveProjectTriggered);
    connect(ui->MenuWidget, &menuWidget::recentProjectTriggered,
            this, &MainWidget::onRecentProjectTriggered);

    // ---- 编辑 ----
    connect(ui->MenuWidget, &menuWidget::selectAllChannelsTriggered,
            this, &MainWidget::onSelectAllChannelsTriggered);
    connect(ui->MenuWidget, &menuWidget::unselectAllChannelsTriggered,
            this, &MainWidget::onUnselectAllChannelsTriggered);

    // ---- 设备 ----
    connect(ui->MenuWidget, &menuWidget::refreshPortTriggered,
            ui->comSetWidget, &ComSetWidget::refreshPorts);
    connect(ui->MenuWidget, &menuWidget::openPortTriggered,
            this, &MainWidget::onOpenPortRequested);
    connect(ui->MenuWidget, &menuWidget::closePortTriggered,
            this, &MainWidget::onClosePortRequested);
    connect(ui->MenuWidget, &menuWidget::startCollectTriggered,
            this, &MainWidget::onStartCollectRequested);
    connect(ui->MenuWidget, &menuWidget::stopCollectTriggered,
            this, &MainWidget::onStopCollectRequested);
    connect(ui->MenuWidget, &menuWidget::channelManagerTriggered,
            this, &MainWidget::onChannelManagerTriggered);
    connect(ui->MenuWidget, &menuWidget::alarmRuleTriggered,
            this, &MainWidget::onAlarmRuleTriggered);

    // ---- 视图 ----
    connect(ui->MenuWidget, &menuWidget::leftPanelToggled,
            this, &MainWidget::onLeftPanelToggled);
    connect(ui->MenuWidget, &menuWidget::realCurveToggled,
            this, &MainWidget::onRealCurveToggled);
    connect(ui->MenuWidget, &menuWidget::realDataToggled,
            this, &MainWidget::onRealDataToggled);
    connect(ui->MenuWidget, &menuWidget::rightPanelToggled,
            this, &MainWidget::onRightPanelToggled);
    connect(ui->MenuWidget, &menuWidget::curveSettingsTriggered,
            this, &MainWidget::onCurveSettingsTriggered);

    // ---- 工具 / 帮助 ----
    connect(ui->MenuWidget, &menuWidget::openDataDirTriggered,
            this, &MainWidget::onOpenDataDirTriggered);
    connect(ui->MenuWidget, &menuWidget::commLogTriggered,
            this, &MainWidget::onCommLogTriggered);
    connect(ui->MenuWidget, &menuWidget::retentionTriggered,
            this, &MainWidget::onRetentionTriggered);
    connect(ui->MenuWidget, &menuWidget::aboutTriggered,
            this, &MainWidget::onAboutTriggered);
    connect(ui->MenuWidget, &menuWidget::quickStartTriggered,
            this, &MainWidget::onQuickStartTriggered);

    // ---- 历史查询入口：实时数据表双击 / 数据存储卡片查询 ----
    connect(ui->realDataWidget, &RealDataWidget::historyRequested,
            this, &MainWidget::onHistoryRequested);
    connect(ui->dataSaveWidget, &DataSaveWidget::historyQueryRequested,
            this, &MainWidget::onHistoryQueryRequestedFromCard);

    // ---- 通道卡片落库开关 / 曲线卡片暂停状态 ----
    connect(ui->channelSetWidget, &ChannelSetWidget::saveToDatabaseToggled,
            this, &MainWidget::onSaveToDatabaseToggled);
    connect(ui->realCurveWidget, &RealCurveWidget::pauseStateChanged,
            this, &MainWidget::onCurvePauseStateChanged);

    refreshActionStates();
}

// ============== UI 请求下发槽 ==============

void MainWidget::onOpenPortRequested()
{
    // 从 ComSetWidget 读取完整串口配置，跨线程发送给 SerialWorker::openPort
    const SerialPortConfig config = ui->comSetWidget->getSerialPortConfig();
    QMetaObject::invokeMethod(m_worker, "openPort", Qt::QueuedConnection,
                              Q_ARG(SerialPortConfig, config));
}

void MainWidget::onClosePortRequested()
{
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::QueuedConnection);
}

void MainWidget::onStartCollectRequested()
{
    // 前置校验：通道地址必须连续（Poller 按连续段读取）
    QString errorMsg;
    if (!m_channelModel->validateAddressContinuity(&errorMsg)) {
        ui->OperationWidget->setStatusText(errorMsg);
        return;
    }

    // 汇总 Modbus 参数（全部从 modbusSetWidget 一站式读取）
    const quint8 slaveId = ui->modbusSetWidget->currentSlaveId();
    const ModbusFunctionCode funcCode = ui->modbusSetWidget->currentFunctionCode();
    const quint16 startAddr = ui->modbusSetWidget->currentStartAddress();
    const quint16 quantity = ui->modbusSetWidget->currentQuantity();
    const int intervalMs = ui->modbusSetWidget->currentIntervalMs();

    // 新一轮采集：统计清零
    ui->collectionStatisticsWidget->resetStatistics();

    // 串口卡片的超时/重试随本次启动下发，后者排队执行在 startPolling 之前
    QMetaObject::invokeMethod(m_worker, "applyCommunicationOptions", Qt::QueuedConnection,
                              Q_ARG(int, ui->comSetWidget->currentResponseTimeoutMs()),
                              Q_ARG(int, ui->comSetWidget->currentRetryCount()));

    QMetaObject::invokeMethod(m_worker, "startPolling", Qt::QueuedConnection,
                              Q_ARG(quint8, slaveId),
                              Q_ARG(ModbusFunctionCode, funcCode),
                              Q_ARG(quint16, startAddr),
                              Q_ARG(quint16, quantity),
                              Q_ARG(int, intervalMs));
}

void MainWidget::onStopCollectRequested()
{
    QMetaObject::invokeMethod(m_worker, "stopPolling", Qt::QueuedConnection);
}

// ============== 工具条 / 菜单动作槽 ==============

void MainWidget::onExportCsvClicked()
{
    // 复用数据存储卡片的时间范围与导出流程
    ui->dataSaveWidget->requestExport();
}

void MainWidget::onScreenshotClicked()
{
    saveScreenshot();
}

void MainWidget::saveScreenshot()
{
    const QString defaultName = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("hmi_%1.png").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    const QString filePath = QFileDialog::getSaveFileName(
        this, LANGUAGE("LG_screenshot_title"), defaultName, LANGUAGE("LG_screenshot_filter"));
    if (filePath.isEmpty()) {
        return;
    }

    if (grab().save(filePath)) {
        QMessageBox::information(this, LANGUAGE("LG_screenshot_title"),
                                 LANGUAGE("LG_screenshot_doneFmt").arg(filePath));
    } else {
        QMessageBox::warning(this, LANGUAGE("LG_screenshot_title"),
                             LANGUAGE("LG_screenshot_failFmt").arg(filePath));
    }
}

void MainWidget::onSelectAllChannelsTriggered()
{
    ui->channelSetWidget->setAllChannelsEnabled(true);
}

void MainWidget::onUnselectAllChannelsTriggered()
{
    ui->channelSetWidget->setAllChannelsEnabled(false);
}

void MainWidget::onOpenDataDirTriggered()
{
    // 数据库目录由 SaveDataSQL 创建，这里先兜底 mkpath 再打开
    const QString dirPath = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("data"));
    QDir().mkpath(dirPath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
}

void MainWidget::onAboutTriggered()
{
    QMessageBox::about(this, LANGUAGE("LG_about_title"),
        LANGUAGE("LG_about_text").arg(LANGUAGE("LG_appVersion"),
                                      QString::fromLatin1(qVersion())));
}

void MainWidget::onLeftPanelToggled(bool visible)
{
    ui->leftPanelWidget->setVisible(visible);
}

void MainWidget::onRealCurveToggled(bool visible)
{
    ui->realCurveWidget->setVisible(visible);
}

void MainWidget::onRealDataToggled(bool visible)
{
    ui->realDataWidget->setVisible(visible);
}

void MainWidget::onRightPanelToggled(bool visible)
{
    ui->rightPanelWidget->setVisible(visible);
}

void MainWidget::onSaveToDatabaseToggled(bool enabled)
{
    m_saveToDatabase = enabled;
}

void MainWidget::onCurvePauseToggled(bool paused)
{
    ui->realCurveWidget->setPaused(paused);
}

void MainWidget::onCurvePauseStateChanged(bool paused)
{
    // 曲线卡片自己点了暂停/恢复时，同步顶部按钮文字
    ui->OperationWidget->updateCurvePauseState(paused);
}

void MainWidget::refreshActionStates()
{
    ui->MenuWidget->updateActionStates(m_portOpen, m_polling);
}

// ============== SerialWorker 状态上收槽 ==============

void MainWidget::onWorkerPortOpened(const QString &portName)
{
    Q_UNUSED(portName);
    m_portOpen = true;
    ui->OperationWidget->updateConnectionState(true);
    refreshActionStates();
}

void MainWidget::onWorkerPortClosed(const QString &portName)
{
    Q_UNUSED(portName);
    m_portOpen = false;
    m_polling = false;
    ui->OperationWidget->updateConnectionState(false);
    ui->OperationWidget->updatePollingState(false);
    refreshActionStates();
}

void MainWidget::onWorkerPortError(QSerialPort::SerialPortError error, const QString &message)
{
    Q_UNUSED(error);
    // 串口层异常 → 记入报警/故障卡片
    ui->alarmAndfaultWidget->addAlarm(AlarmLevel::Fault, message);
    ui->OperationWidget->setStatusText(message);
}

void MainWidget::onWorkerReconnectStateChanged(bool reconnecting, int attempt)
{
    if (reconnecting) {
        ui->OperationWidget->setStatusText(
            QStringLiteral("● ") + LANGUAGE("LG_tool_statusReconnecting")
            + QStringLiteral(" (%1)").arg(attempt));

        // 断线重连 → 记入报警/故障卡片（相同内容 5s 内自动去抖）
        ui->alarmAndfaultWidget->addAlarm(
            AlarmLevel::Fault,
            LANGUAGE("LG_alarm_msg_reconnecting").arg(attempt));
    }
}

void MainWidget::onWorkerPollingStateChanged(bool running)
{
    m_polling = running;
    ui->OperationWidget->updatePollingState(running);
    refreshActionStates();
}

void MainWidget::onWorkerPollFailed(ModbusError error, const QString &message)
{
    // 采集统计：本轮最终失败按错误类型计数
    ui->collectionStatisticsWidget->onRoundFailed(error);
    // 轮询失败 → 记入报警/故障卡片（超时/异常码/CRC 均归为故障）
    ui->alarmAndfaultWidget->addAlarm(AlarmLevel::Fault, message);
    ui->OperationWidget->setStatusText(message);
}

void MainWidget::onWorkerDataUpdated(quint16 startAddress, const QVector<quint16> &values, qint64 rttMs)
{
    // 采集统计：本轮成功，累计请求数与 RTT
    ui->collectionStatisticsWidget->onRoundSuccess(rttMs);

    // 实时数据表：同一轮请求共用一个往返耗时，逐行刷新 RTT 列
    ui->realDataWidget->setRoundTripTime(static_cast<int>(rttMs));

    // 一轮采集完成 → 批量更新 ChannelModel 里的通道原始值
    m_channelModel->updateRawValues(startAddress, values);

    // 阈值判定：连续越限达到去抖次数才记录报警，恢复后记录解除
    evaluateAlarmThresholds();

    // 曲线本轮所有通道已追加完毕，统一重绘一次（避免每通道各重绘一次）
    ui->realCurveWidget->endRoundUpdate();

    // 落库：整理本轮覆盖的通道（通道序号 + 原始值 + 工程值），跨线程投递给数据库线程
    QVector<SampleChannelData> rows;
    const int endAddress = static_cast<int>(startAddress) + static_cast<int>(values.size());
    for (int index = 0; index < m_channelModel->channelCount(); ++index) {
        const ChannelItem &item = m_channelModel->channelAt(index);
        if (item.registerAddress < startAddress || item.registerAddress >= endAddress) {
            continue;
        }
        SampleChannelData row;
        row.channel = index;
        row.raw = item.rawValue;
        row.value = item.value();
        rows.append(row);
    }

    if (m_saveToDatabase && !rows.isEmpty()) {
        QMetaObject::invokeMethod(m_saveData, "enqueueSample", Qt::QueuedConnection,
                                  Q_ARG(qint64, QDateTime::currentMSecsSinceEpoch()),
                                  Q_ARG(QVector<SampleChannelData>, rows));
    }
}

void MainWidget::onDatabaseError(const QString &message)
{
    // 只走状态栏，避免 addAlarm → 落库失败 → 再 addAlarm 的递归
    ui->OperationWidget->setStatusText(message);
}

// ============== 通道管理 / 报警规则 / 通信日志 / 历史查询 ==============

void MainWidget::onChannelManagerTriggered()
{
    ChannelEditDialog dialog(m_channelModel, this);
    dialog.exec();
}

void MainWidget::onAlarmRuleTriggered()
{
    AlarmRuleDialog dialog(m_channelModel, m_alarmDebounceCount, this);
    connect(&dialog, &AlarmRuleDialog::debounceCountChanged,
            this, &MainWidget::onAlarmDebounceChanged);
    dialog.exec();
}

void MainWidget::onAlarmDebounceChanged(int count)
{
    m_alarmDebounceCount = qMax(1, count);
    if (m_projectConfig != nullptr) {
        m_projectConfig->setAlarmDebounceCount(m_alarmDebounceCount);
    }
    // 规则变化后清空累计状态，避免沿用旧阈值下的连续次数
    m_alarmStates.clear();
    updateAlarmParamTip();
}

void MainWidget::onChannelListChanged()
{
    // 通道增删改后下标可能错位，直接丢弃全部越限状态
    m_alarmStates.clear();
}

// ============== 工程文件 ==============

ProjectConfig::SerialSettings MainWidget::currentSerialSettings() const
{
    const SerialPortConfig serialConfig = ui->comSetWidget->getSerialPortConfig();

    ProjectConfig::SerialSettings serial;
    serial.portName = serialConfig.portName;
    serial.baudRate = serialConfig.baudRate;
    serial.dataBits = static_cast<int>(serialConfig.dataBits);
    serial.parity = static_cast<int>(serialConfig.parity);
    serial.stopBits = static_cast<int>(serialConfig.stopBits);
    serial.responseTimeoutMs = ui->comSetWidget->currentResponseTimeoutMs();
    serial.retryCount = ui->comSetWidget->currentRetryCount();
    return serial;
}

ProjectConfig::ModbusSettings MainWidget::currentModbusSettings() const
{
    ProjectConfig::ModbusSettings modbus;
    modbus.functionCodeIndex = ui->modbusSetWidget->currentFunctionCodeIndex();
    modbus.startAddress = ui->modbusSetWidget->currentStartAddress();
    modbus.quantity = ui->modbusSetWidget->currentQuantity();
    modbus.slaveId = ui->modbusSetWidget->currentSlaveId();
    modbus.intervalMs = ui->modbusSetWidget->currentIntervalMs();
    return modbus;
}

QJsonObject MainWidget::currentProjectJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("app"), QStringLiteral("modbus-hmi"));
    root.insert(QStringLiteral("version"), 1);

    const ProjectConfig::SerialSettings serial = currentSerialSettings();
    QJsonObject serialObj;
    serialObj.insert(QStringLiteral("portName"), serial.portName);
    serialObj.insert(QStringLiteral("baudRate"), serial.baudRate);
    serialObj.insert(QStringLiteral("dataBits"), serial.dataBits);
    serialObj.insert(QStringLiteral("parity"), serial.parity);
    serialObj.insert(QStringLiteral("stopBits"), serial.stopBits);
    serialObj.insert(QStringLiteral("responseTimeoutMs"), serial.responseTimeoutMs);
    serialObj.insert(QStringLiteral("retryCount"), serial.retryCount);
    root.insert(QStringLiteral("serial"), serialObj);

    const ProjectConfig::ModbusSettings modbus = currentModbusSettings();
    QJsonObject modbusObj;
    modbusObj.insert(QStringLiteral("functionCodeIndex"), modbus.functionCodeIndex);
    modbusObj.insert(QStringLiteral("startAddress"), modbus.startAddress);
    modbusObj.insert(QStringLiteral("quantity"), modbus.quantity);
    modbusObj.insert(QStringLiteral("slaveId"), modbus.slaveId);
    modbusObj.insert(QStringLiteral("intervalMs"), modbus.intervalMs);
    root.insert(QStringLiteral("modbus"), modbusObj);

    root.insert(QStringLiteral("alarmDebounceCount"), m_alarmDebounceCount);

    if (m_channelModel != nullptr) {
        const QJsonDocument channelDoc =
            QJsonDocument::fromJson(m_channelModel->toJsonBytes());
        root.insert(QStringLiteral("channels"), channelDoc.object());
    }

    return root;
}

bool MainWidget::applyProjectJson(const QJsonObject &root, QString *errorMessage)
{
    if (m_channelModel == nullptr) {
        return false;
    }

    // ---- 串口参数 ----
    const QJsonObject serialObj = root.value(QStringLiteral("serial")).toObject();
    if (!serialObj.isEmpty()) {
        SerialPortConfig serialConfig;
        serialConfig.portName = serialObj.value(QStringLiteral("portName")).toString();
        serialConfig.baudRate = serialObj.value(QStringLiteral("baudRate")).toInt(9600);
        serialConfig.dataBits = static_cast<QSerialPort::DataBits>(
            serialObj.value(QStringLiteral("dataBits")).toInt(8));
        serialConfig.parity = static_cast<QSerialPort::Parity>(
            serialObj.value(QStringLiteral("parity")).toInt(0));
        serialConfig.stopBits = static_cast<QSerialPort::StopBits>(
            serialObj.value(QStringLiteral("stopBits")).toInt(1));

        ui->comSetWidget->applySettings(
            serialConfig,
            serialObj.value(QStringLiteral("responseTimeoutMs")).toInt(500),
            serialObj.value(QStringLiteral("retryCount")).toInt(3));
    }

    // ---- Modbus 采集参数 ----
    const QJsonObject modbusObj = root.value(QStringLiteral("modbus")).toObject();
    if (!modbusObj.isEmpty()) {
        ui->modbusSetWidget->applySettings(
            modbusObj.value(QStringLiteral("functionCodeIndex")).toInt(0),
            static_cast<quint16>(modbusObj.value(QStringLiteral("startAddress")).toInt(0)),
            static_cast<quint16>(modbusObj.value(QStringLiteral("quantity")).toInt(4)),
            static_cast<quint8>(modbusObj.value(QStringLiteral("slaveId")).toInt(1)),
            modbusObj.value(QStringLiteral("intervalMs")).toInt(500));
    }

    // ---- 报警规则 ----
    m_alarmDebounceCount =
        qBound(1, root.value(QStringLiteral("alarmDebounceCount")).toInt(m_alarmDebounceCount), 100);
    updateAlarmParamTip();

    // ---- 通道配置 ----
    if (!root.contains(QStringLiteral("channels"))) {
        if (errorMessage) {
            *errorMessage = LANGUAGE("LG_project_noChannels");
        }
        return false;
    }

    const QByteArray channelBytes = QJsonDocument(
        root.value(QStringLiteral("channels")).toObject()).toJson();
    QString channelError;
    if (!m_channelModel->fromJsonBytes(channelBytes, &channelError)) {
        if (errorMessage) {
            *errorMessage = channelError;
        }
        return false;
    }

    return true;
}

bool MainWidget::saveProjectToFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, LANGUAGE("LG_project_title"),
                             LANGUAGE("LG_project_saveFailFmt").arg(path));
        return false;
    }

    file.write(QJsonDocument(currentProjectJson()).toJson(QJsonDocument::Indented));
    file.close();

    if (m_projectConfig != nullptr) {
        m_projectConfig->setCurrentProjectPath(path);
        m_projectConfig->addRecentProject(path);
    }
    refreshRecentProjects();
    updateWindowTitle();
    return true;
}

bool MainWidget::loadProjectFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, LANGUAGE("LG_project_title"),
                             LANGUAGE("LG_project_openFailFmt").arg(path));
        return false;
    }

    const QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, LANGUAGE("LG_project_title"),
                             LANGUAGE("LG_project_parseFailFmt").arg(path));
        return false;
    }

    QString errorMessage;
    if (!applyProjectJson(doc.object(), &errorMessage)) {
        QMessageBox::warning(this, LANGUAGE("LG_project_title"), errorMessage);
        return false;
    }

    if (m_projectConfig != nullptr) {
        m_projectConfig->setCurrentProjectPath(path);
        m_projectConfig->addRecentProject(path);
    }

    persistProjectConfig();
    refreshRecentProjects();
    updateWindowTitle();
    return true;
}

void MainWidget::onNewProjectTriggered()
{
    // 采集中切换工程会打乱通道映射，先要求停止采集
    if (m_polling) {
        ui->OperationWidget->setStatusText(LANGUAGE("LG_project_needStop"));
        return;
    }

    const auto answer = QMessageBox::question(
        this, LANGUAGE("LG_project_title"), LANGUAGE("LG_project_newConfirm"));
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_channelModel->clear();
    seedDemoChannels();
    m_alarmDebounceCount = 3;
    m_alarmStates.clear();
    updateAlarmParamTip();

    if (m_projectConfig != nullptr) {
        m_projectConfig->setCurrentProjectPath(QString());
    }
    persistProjectConfig();
    updateWindowTitle();
}

void MainWidget::onOpenProjectTriggered()
{
    if (m_polling) {
        ui->OperationWidget->setStatusText(LANGUAGE("LG_project_needStop"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, LANGUAGE("LG_project_openTitle"), QString(),
        LANGUAGE("LG_project_filter"));
    if (path.isEmpty()) {
        return;
    }

    loadProjectFromFile(path);
}

void MainWidget::onSaveProjectTriggered()
{
    QString path;
    if (m_projectConfig != nullptr) {
        path = m_projectConfig->currentProjectPath();
    }

    // 尚未关联工程文件时先选择保存位置
    if (path.isEmpty()) {
        const QString defaultName = QDir(QCoreApplication::applicationDirPath())
                                        .filePath(QStringLiteral("hmi_project.mhproj"));
        path = QFileDialog::getSaveFileName(
            this, LANGUAGE("LG_project_saveTitle"), defaultName,
            LANGUAGE("LG_project_filter"));
        if (path.isEmpty()) {
            return;
        }
    }

    if (saveProjectToFile(path)) {
        persistProjectConfig();
        ui->OperationWidget->setStatusText(LANGUAGE("LG_project_savedFmt").arg(path));
    }
}

void MainWidget::onRecentProjectTriggered(const QString &path)
{
    if (path.isEmpty() || m_polling) {
        return;
    }
    loadProjectFromFile(path);
}

void MainWidget::refreshRecentProjects()
{
    if (m_projectConfig == nullptr) {
        return;
    }
    ui->MenuWidget->setRecentProjects(m_projectConfig->recentProjects());
}

void MainWidget::updateWindowTitle()
{
    QString title = LANGUAGE("LG_appVersion");
    const QString path = (m_projectConfig != nullptr)
                             ? m_projectConfig->currentProjectPath()
                             : QString();
    if (!path.isEmpty()) {
        title = QStringLiteral("%1 - %2").arg(title, QFileInfo(path).fileName());
    }
    setWindowTitle(title);
}

// ============== 曲线设置 / 数据保留 / 快速上手 ==============

void MainWidget::onCurveSettingsTriggered()
{
    int lineWidth = ui->realCurveWidget->lineWidth();
    bool limitVisible = true;
    double limitValue = 80.0;
    if (m_projectConfig != nullptr) {
        lineWidth = m_projectConfig->curveLineWidth();
        limitVisible = m_projectConfig->curveUpperLimitVisible();
        limitValue = m_projectConfig->curveUpperLimit();
    }

    CurveSettingsDialog dialog(lineWidth, limitVisible, limitValue, this);
    if (dialog.exec() == QDialog::Accepted) {
        applyCurveSettings(dialog.lineWidth(), dialog.upperLimitVisible(),
                           dialog.upperLimitValue());
    }
}

void MainWidget::applyCurveSettings(int lineWidth, bool limitVisible, double limitValue)
{
    ui->realCurveWidget->setLineWidth(lineWidth);
    ui->realCurveWidget->setUpperLimitVisible(limitVisible);
    ui->realCurveWidget->setUpperLimitValue(limitValue);

    if (m_projectConfig != nullptr) {
        m_projectConfig->setCurveLineWidth(lineWidth);
        m_projectConfig->setCurveUpperLimitVisible(limitVisible);
        m_projectConfig->setCurveUpperLimit(limitValue);
        m_projectConfig->save();
    }
}

void MainWidget::onRetentionTriggered()
{
    const int currentDays = (m_projectConfig != nullptr)
                                ? m_projectConfig->dataRetentionDays()
                                : 0;

    bool ok = false;
    const int days = QInputDialog::getInt(
        this, LANGUAGE("LG_retention_title"), LANGUAGE("LG_retention_label"),
        currentDays, 0, 3650, 1, &ok);
    if (!ok) {
        return;
    }

    if (m_projectConfig != nullptr) {
        m_projectConfig->setDataRetentionDays(days);
        m_projectConfig->save();
    }

    // 0 天表示关闭自动清理
    if (days > 0) {
        QMetaObject::invokeMethod(m_saveData, "purgeOlderThan", Qt::QueuedConnection,
                                  Q_ARG(int, days));
        ui->OperationWidget->setStatusText(LANGUAGE("LG_retention_doneFmt").arg(days));
    } else {
        ui->OperationWidget->setStatusText(LANGUAGE("LG_retention_off"));
    }
}

void MainWidget::onQuickStartTriggered()
{
    QMessageBox::information(this, LANGUAGE("LG_quickStart_title"),
                             LANGUAGE("LG_quickStart_text"));
}

void MainWidget::onCommLogTriggered()
{
    ensureCommLogDialog();
    m_commLogDialog->show();
    m_commLogDialog->raise();
    m_commLogDialog->activateWindow();
}

void MainWidget::ensureHistoryDialog()
{
    if (m_historyDialog != nullptr || m_channelModel == nullptr || m_saveData == nullptr) {
        return;
    }

    m_historyDialog = new HistoryQueryDialog(m_channelModel, this);
    connect(m_historyDialog, &HistoryQueryDialog::queryRequested,
            this, &MainWidget::onHistoryQueryRequested);
    connect(m_saveData, &SaveDataSQL::samplesQueried,
            m_historyDialog, &HistoryQueryDialog::onSamplesQueried,
            Qt::QueuedConnection);
}

void MainWidget::ensureCommLogDialog()
{
    if (m_commLogDialog != nullptr || m_worker == nullptr) {
        return;
    }

    m_commLogDialog = new CommLogDialog(this);
    connect(m_worker, &SerialWorker::frameLogged,
            m_commLogDialog, &CommLogDialog::onFrameLogged, Qt::QueuedConnection);
}

void MainWidget::onHistoryRequested(int channelIndex)
{
    ensureHistoryDialog();
    if (m_historyDialog == nullptr) {
        return;
    }

    // 默认查询最近 1 小时
    const qint64 toMs = QDateTime::currentMSecsSinceEpoch();
    m_historyDialog->openWithRange(toMs - 3600LL * 1000LL, toMs, channelIndex);
}

void MainWidget::onHistoryQueryRequestedFromCard(qint64 fromMs, qint64 toMs)
{
    ensureHistoryDialog();
    if (m_historyDialog == nullptr) {
        return;
    }

    m_historyDialog->openWithRange(fromMs, toMs, -1);
}

void MainWidget::onHistoryQueryRequested(qint64 fromMs, qint64 toMs, int channelIndex)
{
    if (m_saveData == nullptr) {
        return;
    }

    // 由数据库线程执行查询，结果通过 samplesQueried 回到对话框
    QMetaObject::invokeMethod(m_saveData, "querySamples", Qt::QueuedConnection,
                              Q_ARG(qint64, fromMs), Q_ARG(qint64, toMs),
                              Q_ARG(int, channelIndex), Q_ARG(int, 5000));
}

// ============== 报警阈值判定 ==============

void MainWidget::evaluateAlarmThresholds()
{
    if (m_channelModel == nullptr) {
        return;
    }

    const int count = m_channelModel->channelCount();
    if (m_alarmStates.size() != count) {
        m_alarmStates.clear();
        m_alarmStates.resize(count);
    }

    const int debounce = qMax(1, m_alarmDebounceCount);
    for (int index = 0; index < count; ++index) {
        const ChannelItem &item = m_channelModel->channelAt(index);
        ChannelAlarmState &state = m_alarmStates[index];

        // 未启用通道/报警，或阈值配置非法时不做判定
        if (!item.enabled || !item.alarmEnabled || item.alarmHigh <= item.alarmLow) {
            state.overCount = 0;
            state.active = false;
            continue;
        }

        const double value = item.value();
        const bool overHigh = value > item.alarmHigh;
        const bool belowLow = value < item.alarmLow;

        // 恢复正常：已记录过报警才补一条解除记录
        if (!overHigh && !belowLow) {
            if (state.active) {
                state.active = false;
                ui->alarmAndfaultWidget->addAlarm(
                    AlarmLevel::Alarm,
                    LANGUAGE("LG_alarm_msg_recoverFmt")
                        .arg(item.name, alarmValueText(item, value)));
            }
            state.overCount = 0;
            continue;
        }

        // 报警中不重复累计，避免持续越限刷屏
        if (state.active) {
            continue;
        }

        ++state.overCount;
        if (state.overCount < debounce) {
            continue;
        }

        state.active = true;
        state.overCount = 0;
        raiseChannelAlarm(item, overHigh, value);
    }
}

void MainWidget::raiseChannelAlarm(const ChannelItem &item, bool overHigh, double value)
{
    const QString limitText = alarmValueText(
        item, overHigh ? item.alarmHigh : item.alarmLow);
    const QString valueText = alarmValueText(item, value);

    const QString message = overHigh
        ? LANGUAGE("LG_alarm_msg_overHighFmt").arg(item.name, limitText, valueText)
        : LANGUAGE("LG_alarm_msg_belowLowFmt").arg(item.name, limitText, valueText);

    ui->alarmAndfaultWidget->addAlarm(AlarmLevel::Alarm, message);
}

QString MainWidget::alarmValueText(const ChannelItem &item, double value) const
{
    const QString number = QString::number(value, 'f', qBound(0, item.decimals, 6));
    return item.unit.isEmpty() ? number : QStringLiteral("%1 %2").arg(number, item.unit);
}

void MainWidget::updateAlarmParamTip()
{
    if (ui->alarmAndfaultWidget == nullptr) {
        return;
    }

    ui->alarmAndfaultWidget->setParamTip(
        LANGUAGE("LG_alarm_paramTipFmt").arg(m_alarmDebounceCount));
}

// ============== 事件处理 ==============

void MainWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

// 重新实现事件处理函数，处理语言变化
void MainWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        LanguageType languageType = Language::getInstance()->convertToLanguageType(
            QLocale::system().language());
        Language::getInstance()->setLanguageType(languageType);
    } else {
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
