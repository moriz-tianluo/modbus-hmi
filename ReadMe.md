# Modbus HMI · 工业上位机采集监控

基于 **Qt 6 Widgets** 的 Modbus RTU 串口采集上位机。通过串口周期读取从站寄存器，将原始值换算为工程值，并以实时数据表、归一化曲线、报警记录、SQLite 历史数据等形式呈现；同时提供工程文件、通道配置、通信日志与数据导出等完整工具链。

> 版本：v0.9　｜　Qt 6.8.3　｜　C++17　｜　CMake ≥ 3.19

---

## 1. 项目简介

本项目用于演示与落地一套完整的工业采集上位机：

- 单主站、多从站串行事务的 **Modbus RTU** 通信（CRC16、3.5 字符判帧、超时重试、异常码解析、断线重连、05 / 06 / 0F / 10 写操作）
- 多通道寄存器采集，支持无符号/有符号 16 位、缩放系数、量程、单位与小数位配置
- 实时数据表 + QCustomPlot 实时曲线（Y 轴 0~100 归一化，X 轴可缩放）
- 通道越限报警、采集链路故障记录、去抖与恢复判定
- SQLite 采样落库、历史查询、按时间范围统计、CSV 导出、按天自动清理
- `.mhproj` 工程文件的新建 / 打开 / 保存 / 最近工程
- 中英文语言包（XML），界面文本统一由 `Language` 类提供，支持运行时切换并持久化
- 工程脏标记与单实例保护，避免配置丢失与重复启动
- 独立的串口采集线程与数据库线程，UI 线程不阻塞

配套 `python/slave_sim.py` 可模拟从站，用于无硬件环境下的联调；`tool/sscom` 为常用的串口调试助手。

---

## 2. 功能特性

### 2.1 通信与采集

| 能力 | 说明 |
|---|---|
| 串口管理 | 自动枚举端口，支持波特率、数据位、校验位、停止位配置 |
| Modbus RTU | 组帧 / CRC16 校验 / 应答解析 / 异常码识别，读支持 03 / 04（界面开放），协议层同时实现 01 / 02 解析；写支持 05 / 06 / 0F / 10，写应答回显校验后才判定成功 |
| 周期轮询 | 按通道配置自动生成「从站 + 连续地址段」任务列表，支持多从站、地址断档多段轮询；单事务串行，避免并发请求冲突 |
| 写操作 | 设备菜单「写寄存器 / 线圈」：支持单/多寄存器、单/多线圈；必须勾选写保护并二次确认后下发；写入期间自动暂停轮询，完成后恢复 |
| 超时与重试 | 单帧响应超时、帧级重试次数可配；轮询层与协议层重试互不叠加，语义清晰 |
| 帧判据 | 按 3.5 字符时间判帧（高波特率自动缩短），可处理半包 / 粘包 |
| 断线重连 | 串口异常断开后自动重连（默认间隔 2 s、最多 10 次），重连成功可自动恢复采集 |
| 通信日志 | TX / RX 帧级日志，HEX 显示，可暂停、清空、保存 |

### 2.2 数据展示

| 模块 | 说明 |
|---|---|
| 通道配置 | 通道增删改，配置名称、从站地址、寄存器地址、数据类型、单位、换算系数、量程、小数位、颜色、启用状态、报警上下限；不同从站可共用同一寄存器地址 |
| 实时数据表 | 显示通道、寄存器、原始值、工程值、单位、RTT、状态、更新时间；停用通道整行置灰；越限时显示「超上限 / 低于下限」并将整行标红 |
| 实时曲线 | 最多同时绘制 8 条通道曲线（前 4 条常驻图例，超出自动补充），按量程归一化到 0~100%；时间窗口可选 1 分钟 / 5 分钟 / 15 分钟 / 1 小时 / 4 小时；支持暂停、X 轴滚轮缩放与拖动、双击恢复跟随最新（Y 轴固定 0~100，不跟随缩放） |
| 曲线读数 | 鼠标在图表上移动时显示竖直十字光标与左上角读数，按通道给出光标处的工程值 |
| 工程量副轴 | 右侧副轴按高亮通道（或首个启用通道）的量程显示工程刻度，左轴保持 0~100% 归一化 |
| 图例交互 | 单击图例高亮对应通道并与数据表联动；双击图例隐藏 / 显示该曲线 |
| 上限参考线 | 可配置显示开关、数值（0~100）与曲线线宽 |
| 曲线导出 | 工具菜单支持保存曲线截图（PNG）与导出当前曲线缓冲数据（CSV，长表格式） |
| 采集统计 | 总请求、成功率、超时、异常码、平均 RTT、峰值 RTT、CRC 错误、丢点率 |

### 2.3 报警与故障

- 统一收集串口错误、轮询失败、断线重连、通道越限等事件
- 通道报警按「连续越限 N 次」去抖（默认 3 次，可配置），恢复后记录解除
- 卡片固定展示最新 3 条，右上角显示总条数；「查看全部」弹窗展示全部记录，最多保留 200 条
- 相同内容 5 秒内重复触发只刷新时间，避免刷屏
- 新报警触发系统提示音与托盘气泡提醒，窗口最小化时也能感知
- 报警记录同步写入 SQLite `alarm` 表，并记录确认状态
- 工具菜单「报警记录」：按时间范围查询报警表，显示总数 / 报警数 / 故障数 / 未确认数，支持「确认选中」「全部确认」与 CSV 导出

### 2.4 数据存储与查询

| 能力 | 说明 |
|---|---|
| 落库方式 | 独立数据库线程 + SQLite（WAL 模式），每累计 50 个采样轮次批量事务提交，退出前强制落盘 |
| 记录统计 | 按时间范围统计采样条数；实时数据卡片底部脚注定时显示数据库总记录数 |
| 历史查询 | 选择起止时间与通道，查看结果表格与趋势图；实时数据表双击行可直接带入该通道；单次查询上限 5000 行，达到上限时结果区给出红色截断提示 |
| CSV 导出 | 按时间范围导出，可选字段：时间 / 通道 / 原始值 / 工程值；UTF-8 带 BOM，Excel 打开中文不乱码 |
| 数据清理 | 可设置保留天数（0 表示不自动清理），启动时与手动设置时清理过期采样与报警记录 |
| 索引优化 | `sample(ts)` 与 `sample(channel, ts)` 复合索引；按通道 + 时间筛选走复合索引，避免全表扫描 |
| 版本迁移 | 通过 `PRAGMA user_version` 记录结构版本，启动时自动补齐索引与 `alarm.acknowledged` 列 |
| 维护 | 工具菜单「备份数据库」（VACUUM INTO 生成独立备份）与「整理数据库」（VACUUM 回收碎片） |

### 2.5 工程与配置

- **工程文件**：`*.mhproj`（JSON），包含串口参数、Modbus 采集参数、报警去抖次数、全部通道配置
- **新建 / 打开 / 保存 / 最近工程**：最近工程最多保留 5 条，再次打开自动回填
- **脏标记与保存确认**：工程内容变更后标题栏追加 `*`；新建、打开、关闭窗口前若有未保存修改，会提示保存 / 放弃 / 取消
- **运行配置**：`data/project.ini` 记忆窗口几何、串口、Modbus、落库开关、缓冲点数、曲线窗口、报警去抖、数据保留天数、曲线外观等
- **通道配置 JSON**：支持导入 / 导出，导入时校验同一从站内寄存器地址不重复（地址断档会按段轮询）

### 2.6 其他

- 顶部菜单 + 操作工具条 + 系统托盘 + 状态栏
- 视图显隐：左栏、实时曲线、实时数据、右栏
- 界面截图保存（PNG，文件名带时间戳）
- 一键打开数据目录
- 设备菜单：通道管理、报警规则、写寄存器 / 线圈
- 工具菜单：报警记录、备份数据库、整理数据库、曲线截图、曲线数据导出、通信日志
- 快速上手说明与关于对话框
- 界面语言可在「帮助 → 语言」中运行时切换（跟随系统 / 简体中文 / English），选择结果写入 `project.ini`
- 单实例保护：已在运行时重复启动只弹出提示，不再创建第二个主窗口
- 界面样式由 QSS + `color.ini` 驱动，语言文本由 XML 语言包驱动

---

## 3. 界面与架构

### 3.1 界面布局

```
┌──────────────────────────────────────────────────────────────┐
│ 菜单栏：文件 / 编辑 / 设备 / 视图 / 工具 / 帮助               │
├──────────────────────────────────────────────────────────────┤
│ 操作栏：打开串口 / 开始采集 / 暂停曲线 / 导出 CSV / 截图 / 状态 │
├───────────────┬──────────────────────────┬───────────────────┤
│ 左栏          │ 中栏                      │ 右栏              │
│ · 串口设置    │ · 实时曲线                │ · 采集统计        │
│ · Modbus 设置 │ · 实时数据                │ · 报警与故障      │
│ · 通道配置    │                          │ · 数据存储        │
├───────────────┴──────────────────────────┴───────────────────┤
│ 状态栏：连接信息 / 采集周期 / 成功 / 超时 / 异常 / 记录数     │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 线程模型与数据流

```
UI 线程
  ├─ MainWidget（汇总校验、菜单与对话框、工程文件）
  ├─ ChannelModel（通道数据与 JSON 序列化）
  └─ UI 控件（配置卡片 / 数据表 / 曲线 / 报警 / 存储）

采集线程（QThread）
  └─ SerialWorker
       ├─ SerialPortHelper   串口收发、判帧、重连
       ├─ ModbusRtu          Modbus 组帧 / 解帧 / CRC / 重试 / 写应答校验
       └─ ModbusPoller       任务列表调度、防并发

数据库线程（QThread）
  └─ SaveDataSQL           批量事务、查询、导出、清理、备份 / VACUUM
```

采集结果经 `SerialWorker::dataUpdated` 跨线程回到 UI 线程，写入 `ChannelModel` 后再分发到数据表与曲线；同时按需投递到数据库线程落库。

轮询任务由通道配置生成：`ChannelModel::buildPollTasks()` 按 `(从站, 功能码)` 归集启用地址，排序去重后按「地址连续 + 单次读取上限」切段，得到 `PollTask{slaveId, functionCode, startAddress, quantity}` 列表交给 `ModbusPoller` 依次执行，因此不同从站、地址断档的通道可以在同一采集周期内完成。

---

## 4. 技术栈

| 类别 | 技术 |
|---|---|
| 界面框架 | Qt 6.8.3 Widgets（Core / Widgets / Xml / Charts / SerialPort / Sql / PrintSupport） |
| 曲线绘制 | QCustomPlot 2.x（内置 `3rdparty/QCustomPlot`） |
| 通信协议 | Modbus RTU（自实现组帧、CRC16、解析、重试） |
| 数据存储 | SQLite（Qt SQL 驱动），WAL 模式 |
| 配置持久化 | QSettings（INI） + JSON |
| 构建系统 | CMake ≥ 3.19，支持 MinGW / MSVC |
| 语言标准 | C++17 |

---

## 5. 目录结构

```
modbus-hmi/
├─ CMakeLists.txt              # 构建脚本（源文件、资源、语言包拷贝）
├─ include/
│  ├─ base/                    # 基类：窗口、托盘、样式、语言、日志、配置、信号
│  ├─ common/                  # 业务核心：串口、Modbus、轮询、通道模型、SQL、工程配置
│  ├─ component/               # 界面卡片与对话框
│  └─ mainwidget.h             # 主窗口
├─ src/                        # 与 include 对应的实现
├─ forms/                      # Qt Designer .ui 文件
├─ language/
│  ├─ chinese.xml              # 中文语言包
│  └─ english.xml              # 英文语言包
├─ resource/                   # 图标、QSS、颜色配置、界面示意图
├─ 3rdparty/QCustomPlot/       # 曲线控件源码
├─ python/slave_sim.py         # Modbus 从站模拟器
├─ tool/sscom/                 # 串口调试助手
└─ document/                   # 功能拆解与实现说明
```

主要业务类：

| 类 | 职责 |
|---|---|
| `MainWidget` | 主窗口与业务中枢：线程编排、信号接线、工程文件、菜单动作 |
| `ChannelModel` | 通道列表、工程值换算、量程归一化、JSON 导入导出、同从站地址校验、轮询任务切分 |
| `SerialWorker` | 采集线程入口，组合串口、协议与轮询三层；读写互斥 |
| `SerialPortHelper` | 串口打开/关闭、异步接收、3.5 字符判帧、断线重连 |
| `ModbusRtu` | 请求组帧、CRC16、应答解析、超时重试、异常码描述、写应答回显校验 |
| `ModbusPoller` | 任务列表调度、轮询状态机、单轮重试 |
| `SaveDataSQL` | SQLite 建表与版本迁移、批量写入、采样/报警查询、CSV 导出、清理、备份与 VACUUM |
| `ProjectConfig` | `project.ini` 读写、最近工程、数据保留、曲线外观 |
| `RealDataWidget` | 实时数据表与越限状态展示 |
| `RealCurveWidget` | QCustomPlot 实时曲线（8 通道）、图例交互、十字光标读数、工程量副轴、截图与数据导出 |
| `AlarmAndFaultWidget` | 报警/故障卡片、去抖、查看全部 |
| `AlarmHistoryDialog` | 报警记录按时间范围查询、确认、统计与 CSV 导出 |
| `WriteRegisterDialog` | 写寄存器 / 线圈下发，写保护与二次确认 |
| `HistoryQueryDialog` | 历史数据查询表格与趋势图 |
| `CommLogDialog` | 收发帧日志 |
| `ChannelEditDialog` / `AlarmRuleDialog` / `CurveSettingsDialog` | 通道属性（含从站地址）、报警规则、曲线设置 |

---

## 6. 构建与运行

### 6.1 环境要求

- Qt **6.8.3**，需包含 `Widgets`、`Xml`、`Charts`、`Sql`、`SerialPort`、`PrintSupport` 模块
- CMake ≥ 3.19
- 编译器：MinGW 64-bit 或 MSVC 2022 64-bit（支持 C++17）
- Python 3 + `pyserial`（仅模拟从站时需要）

### 6.2 命令行构建（MinGW + Ninja）

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="D:/Qt/6.8.3/mingw_64" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 6.3 命令行构建（MSVC 2022）

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Debug
```

> 也可以直接用 **Qt Creator** 打开根目录 `CMakeLists.txt`，选择对应 Qt 6.8.3 Kit 后构建运行。

### 6.4 运行说明

- 构建后 CMake 会自动把 `language/` 拷贝到可执行文件所在目录（多配置生成器下即 `Debug/`、`Release/` 内），语言包按相对路径加载
- 首次运行会在可执行文件目录下自动创建 `data/`，用于存放 `hmi.db`、`project.ini`、`channels.json`
- 若程序目录不可写（例如安装到 `Program Files`），`project.ini` 会退化到系统配置目录
- 程序为单实例运行，重复启动会提示「程序已在运行」

### 6.5 自动化测试

项目已接入 Qt Test + CTest，测试目标默认随构建生成：

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

| 测试目标 | 覆盖内容 |
|---|---|
| `test_modbusrtu` | CRC16 已知向量、组帧、寄存器 / 位 / 异常 / CRC 错误 / 短帧解析、写请求组帧 |
| `test_channelmodel` | U16 / S16 换算、百分比钳位、同从站地址重复校验、地址断档与多从站 JSON 往返、多段轮询任务切分、禁用通道跳过更新 |
| `test_savedatasql` | 采样落库与计数、限条数查询、CSV 字段掩码、过期数据清理、报警查询 / 确认 / 导出、数据库备份与 VACUUM |
| `test_languagekeys` | 中英文语言包键集合一致、无重复键 |

如需关闭测试构建，配置时追加 `-DBUILD_TESTING=OFF`。

---

## 7. 快速上手（无硬件联调）

### 7.1 准备虚拟串口

使用 com0com 等工具创建一对虚拟串口，例如 `COM9 <-> COM10`。

### 7.2 启动模拟从站

```bash
pip install pyserial
python python/slave_sim.py --port COM10 --slave 1 --regs 8
```

模拟器默认在寄存器 0~3 生成四种信号：正弦波动、锯齿递增、随机噪声、常量，便于在曲线上区分。

可用参数：

| 参数 | 说明 |
|---|---|
| `--port` | 串口名，如 `COM10` |
| `--baud` | 波特率，默认 9600 |
| `--slave` | 从站号，默认 1 |
| `--regs` | 可用寄存器数量，默认 8 |
| `--timeout-rate` | 按概率不响应，用于测试超时重试（0~1） |
| `--exc-rate` | 按概率返回 Modbus 异常码 |
| `--bad-crc-rate` | 按概率返回错误 CRC |
| `--slow-ms` | 延迟响应毫秒数，用于测试超时阈值 |
| `--split` | 将应答拆成两半发送，测试半包拼帧 |

### 7.3 连接与采集

1. 启动上位机，左栏选择 `COM9`、`9600`、`8-N-1`，从站号填 `1`
2. 点击「打开串口」→「开始采集」
3. 中栏查看实时曲线与实时数据；双击数据行可查看该通道历史数据
4. 「工具 → 通信日志」查看收发帧，排查通信问题
5. 「文件 → 保存工程」将当前配置保存为 `.mhproj`，下次可直接打开

---

## 8. 配置与数据文件

### 8.1 `data/project.ini`

| 分组 | 键 | 说明 |
|---|---|---|
| `serial` | `portName` / `baudRate` / `dataBits` / `parity` / `stopBits` | 串口参数 |
| `serial` | `responseTimeoutMs` / `retryCount` | 响应超时与重试次数 |
| `modbus` | `functionCodeIndex` / `startAddress` / `quantity` / `slaveId` / `intervalMs` | Modbus 采集参数 |
| `ui` | `windowGeometry` / `channelConfigPath` | 窗口几何与通道配置路径 |
| `ui` | `saveToDatabase` / `bufferSize` / `curveWindowSeconds` / `alarmDebounceCount` | 落库开关、缓冲点数、曲线窗口、报警去抖 |
| `project` | `currentPath` / `recentProjects` | 当前工程与最近工程列表 |
| `project` | `dataRetentionDays` | 数据保留天数（0 为不清理） |
| `project` | `curveLineWidth` / `curveUpperLimit` / `curveUpperLimitVisible` | 曲线外观 |

### 8.2 工程文件 `*.mhproj`

```json
{
  "app": "modbus-hmi",
  "version": 1,
  "serial": {
    "portName": "COM9",
    "baudRate": 9600,
    "dataBits": 8,
    "parity": 0,
    "stopBits": 1,
    "responseTimeoutMs": 500,
    "retryCount": 3
  },
  "modbus": {
    "functionCodeIndex": 0,
    "startAddress": 0,
    "quantity": 4,
    "slaveId": 1,
    "intervalMs": 500
  },
  "alarmDebounceCount": 3,
  "channels": {
    "bufferSize": 2000,
    "channels": [
      {
        "name": "温度",
        "unit": "℃",
        "slave": 1,
        "register": 0,
        "dataType": 0,
        "scale": 0.1,
        "rangeMin": 0.0,
        "rangeMax": 100.0,
        "decimals": 2,
        "color": "#e05a4f",
        "enabled": true,
        "alarmEnabled": true,
        "alarmHigh": 80.0,
        "alarmLow": 10.0
      }
    ]
  }
}
```

### 8.3 通道配置 JSON

结构与 `channels` 字段一致：

```json
{
  "bufferSize": 2000,
  "channels": [
    {
      "name": "压力",
      "unit": "MPa",
      "slave": 2,
      "register": 1,
      "dataType": 0,
      "scale": 0.1,
      "rangeMin": 0.0,
      "rangeMax": 100.0,
      "decimals": 2,
      "color": "#4f8fe0",
      "enabled": true,
      "alarmEnabled": false,
      "alarmHigh": 100.0,
      "alarmLow": 0.0
    }
  ]
}
```

字段说明：

| 字段 | 说明 |
|---|---|
| `slave` | 从站地址（1~247），缺省为 1；不同从站可共用同一寄存器地址 |
| `register` | 寄存器地址（从 0 开始） |
| `dataType` | `0` 无符号 16 位；`1` 有符号 16 位 |
| `scale` | 工程值 = 原始值 × `scale` |
| `rangeMin` / `rangeMax` | 量程，用于曲线归一化与百分比计算 |
| `decimals` | 工程值显示小数位 |
| `color` | 曲线 / 色点颜色，空字符串表示按索引取默认色 |
| `enabled` | 是否参与采集与显示 |
| `alarmEnabled` / `alarmHigh` / `alarmLow` | 越限报警开关与上下限 |

> **重要约束**：同一从站内的寄存器地址不能重复；地址允许断档（例如 0、1、5、6），轮询会按连续段拆成多个任务依次读取。单个任务受 Modbus 单次读取上限约束，超出时自动切段。

> 未手动指定通道配置文件时，程序退出前会把当前通道自动保存到 `data/channels.json`。

### 8.4 SQLite 表结构

数据库文件：`data/hmi.db`

```sql
CREATE TABLE sample (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  ts      INTEGER NOT NULL,   -- 采样时间（Unix 毫秒）
  channel INTEGER NOT NULL,   -- 通道序号
  raw     INTEGER NOT NULL,   -- 原始寄存器值
  value   REAL    NOT NULL    -- 工程值
);
CREATE INDEX idx_sample_ts ON sample(ts);
CREATE INDEX idx_sample_channel_ts ON sample(channel, ts);  -- v2：按通道 + 时间筛选

CREATE TABLE alarm (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  ts           INTEGER NOT NULL,   -- 触发时间（Unix 毫秒）
  level        INTEGER NOT NULL,   -- 0=报警，1=故障
  message      TEXT    NOT NULL,   -- 描述
  acknowledged INTEGER NOT NULL DEFAULT 0  -- v2：确认状态
);
CREATE INDEX idx_alarm_ts ON alarm(ts);
```

结构版本记录在 `PRAGMA user_version`（当前为 `2`）。启动时 `SaveDataSQL::applyMigrations()` 幂等执行迁移：创建 `idx_sample_channel_ts`、为老库补齐 `alarm.acknowledged` 列，再写回版本号；已有数据不受影响。

---

## 9. 快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl+N` | 新建工程 |
| `Ctrl+O` | 打开工程 |
| `Ctrl+S` | 保存工程 |
| `Ctrl+E` | 导出采集数据（CSV） |
| `Ctrl+Q` | 退出 |
| `Ctrl+A` | 全选通道 |
| `F5` | 刷新端口 |
| `Ctrl+K` | 连接（打开串口） |
| `Ctrl+D` | 断开连接 |
| `F9` | 开始采集 |
| `Shift+F10` | 停止采集 |
| `Ctrl+1` ~ `Ctrl+4` | 显示 / 隐藏左栏、实时曲线、实时数据、右栏 |

---

## 10. 国际化

- 语言文本统一通过 `LANGUAGE("KEY")` 获取，禁止在界面代码中直接写死中文
- 语言包位于 `language/chinese.xml`、`language/english.xml`，键值一一对应
- 新增文本时需同时补充中英文两个 XML
- 语言包按相对路径 `language/` 加载，构建后需确保目录与可执行文件同级
- 运行时可在「帮助 → 语言」切换：跟随系统 / 简体中文 / English；切换后遍历所有 `BaseWidget` 统一重译，并将选择写入 `project.ini`；仅在「跟随系统」时响应系统语言变化

---

## 11. 已知限制

- 采集链路以寄存器读取（03 / 04）为主；协议层虽已实现 01 / 02 解析，界面中这两项已禁用，避免误选后无法展示
- 实时曲线最多同时绘制 8 个通道（超出部分仅采集与显示在数据表中），工程量副轴同一时刻只跟随一个通道
- 同一从站内的寄存器地址不能重复；地址断档会自动拆成多段轮询，段数受从站数量与采集周期共同影响
- 写操作写保护为强制勾选，二次确认后才能下发；写入期间会暂停轮询，写失败不会自动重试
- 单串口串行事务轮询：支持多从站，但同一时刻只有一个请求在途，从站较多时单轮周期相应变长

---

## 12. 相关文档

- `document/功能详细.md`：按界面区域拆解的功能清单与优先级
- `document/上位机.md`：环境准备、硬件模拟与实现说明
- `document/知识点.md`：项目涉及的关键知识点
- `resource/ui/上位机界面示意.svg`：界面布局示意
