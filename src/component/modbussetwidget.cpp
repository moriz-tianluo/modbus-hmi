#include "modbussetwidget.h"
#include "ui_modbussetwidget.h"
#include "Language.h"

#include <QStandardItem>
#include <QStandardItemModel>

ModbusSetWidget::ModbusSetWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::ModbusSetWidget)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);
    initConnections();
    disableUnsupportedFunctionCodes();
    languageChange();
}

ModbusSetWidget::~ModbusSetWidget()
{
    delete ui;
}

void ModbusSetWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_modbusSet"));
    ui->labelFuncCode->setText(LANGUAGE("LG_modbus_funcCode"));
    ui->labelStartAddr->setText(LANGUAGE("LG_modbus_startAddr"));
    ui->labelQuantity->setText(LANGUAGE("LG_modbus_quantity"));
    ui->labelSlaveId->setText(LANGUAGE("LG_modbus_slave"));
    ui->labelInterval->setText(LANGUAGE("LG_modbus_interval"));
    ui->labelTip->setText(LANGUAGE("LG_modbus_tip"));
    ui->comboBoxFuncCode->setToolTip(LANGUAGE("LG_modbus_unsupportedFuncTip"));
}

void ModbusSetWidget::initConnections()
{
    connect(ui->comboBoxFuncCode, &QComboBox::currentIndexChanged,
            this, &ModbusSetWidget::settingsChanged);
    connect(ui->comboBoxStartAddr, &QComboBox::currentIndexChanged,
            this, &ModbusSetWidget::settingsChanged);
    connect(ui->comboBoxQuantity, &QComboBox::currentIndexChanged,
            this, &ModbusSetWidget::settingsChanged);
    connect(ui->comboBoxSlaveId, &QComboBox::currentIndexChanged,
            this, &ModbusSetWidget::settingsChanged);
    connect(ui->comboBoxInterval, &QComboBox::currentIndexChanged,
            this, &ModbusSetWidget::settingsChanged);
}

void ModbusSetWidget::disableUnsupportedFunctionCodes()
{
    // 采集链路目前只按寄存器读取，01/02 选项禁用，避免误选后无数据
    QStandardItemModel *model =
        qobject_cast<QStandardItemModel *>(ui->comboBoxFuncCode->model());
    if (model != nullptr) {
        for (int i = 2; i < model->rowCount(); ++i) {
            QStandardItem *item = model->item(i);
            if (item != nullptr) {
                item->setEnabled(false);
            }
        }
    }

    if (ui->comboBoxFuncCode->currentIndex() > 1) {
        ui->comboBoxFuncCode->setCurrentIndex(0);
    }
}

quint8 ModbusSetWidget::currentSlaveId() const
{
    bool ok = false;
    const int value = ui->comboBoxSlaveId->currentText().toInt(&ok);
    return ok && value >= 1 && value <= 247 ? static_cast<quint8>(value) : 1;
}

int ModbusSetWidget::currentIntervalMs() const
{
    // 文本形如 "500 ms"，去掉空格和 "ms" 后解析
    QString text = ui->comboBoxInterval->currentText();
    text.remove(QLatin1Char(' '));
    text.remove(QStringLiteral("ms"));
    bool ok = false;
    const int value = text.toInt(&ok);
    return ok && value >= 50 ? value : 500;
}

ModbusFunctionCode ModbusSetWidget::currentFunctionCode() const
{
    // 下拉框 index: 0=03读保持, 1=04读输入；01/02 已在界面禁用
    switch (ui->comboBoxFuncCode->currentIndex()) {
    case 1: return ModbusFunctionCode::ReadInputRegisters;
    case 0:
    default: return ModbusFunctionCode::ReadHoldingRegisters;
    }
}

int ModbusSetWidget::currentFunctionCodeIndex() const
{
    return qBound(0, ui->comboBoxFuncCode->currentIndex(), 1);
}

quint16 ModbusSetWidget::currentStartAddress() const
{
    const QString text = ui->comboBoxStartAddr->currentText();
    bool ok = false;
    // 十六进制解析（自动识别 0x 前缀）
    const quint16 value = text.toUInt(&ok, 16);
    return ok ? value : 0;
}

quint16 ModbusSetWidget::currentQuantity() const
{
    bool ok = false;
    const quint16 value = ui->comboBoxQuantity->currentText().toUShort(&ok);
    return ok && value > 0 ? value : 4;
}

void ModbusSetWidget::applySettings(int functionCodeIndex, quint16 startAddress,
                                    quint16 quantity, quint8 slaveId, int intervalMs)
{
    if (functionCodeIndex >= 0 && functionCodeIndex < ui->comboBoxFuncCode->count()) {
        // 历史配置可能保存了已禁用的 01/02，回填时收敛到 03/04
        ui->comboBoxFuncCode->setCurrentIndex(qBound(0, functionCodeIndex, 1));
    }

    // 起始地址下拉项为 "0x0000" 形式，不存在时补入以免回填丢失
    const QString addressText =
        QStringLiteral("0x%1").arg(startAddress, 4, 16, QLatin1Char('0'));
    int addressIndex = ui->comboBoxStartAddr->findText(addressText);
    if (addressIndex < 0) {
        ui->comboBoxStartAddr->addItem(addressText);
        addressIndex = ui->comboBoxStartAddr->count() - 1;
    }
    ui->comboBoxStartAddr->setCurrentIndex(addressIndex);

    // 读取数量
    int quantityIndex = ui->comboBoxQuantity->findText(QString::number(quantity));
    if (quantityIndex < 0) {
        ui->comboBoxQuantity->addItem(QString::number(quantity));
        quantityIndex = ui->comboBoxQuantity->count() - 1;
    }
    ui->comboBoxQuantity->setCurrentIndex(quantityIndex);

    // 从站号
    int slaveIndex = ui->comboBoxSlaveId->findText(QString::number(slaveId));
    if (slaveIndex < 0) {
        ui->comboBoxSlaveId->addItem(QString::number(slaveId));
        slaveIndex = ui->comboBoxSlaveId->count() - 1;
    }
    ui->comboBoxSlaveId->setCurrentIndex(slaveIndex);

    // 采集周期下拉项形如 "500 ms"，按数字部分匹配
    for (int i = 0; i < ui->comboBoxInterval->count(); ++i) {
        QString text = ui->comboBoxInterval->itemText(i);
        text.remove(QLatin1Char(' '));
        text.remove(QStringLiteral("ms"));
        bool ok = false;
        const int value = text.toInt(&ok);
        if (ok && value == intervalMs) {
            ui->comboBoxInterval->setCurrentIndex(i);
            break;
        }
    }
}
