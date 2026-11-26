#include "writeregisterdialog.h"
#include "Language.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// 写入类型下拉框索引
enum WriteMode {
    ModeSingleRegister = 0,
    ModeMultipleRegisters = 1,
    ModeSingleCoil = 2,
    ModeMultipleCoils = 3,
};
}

WriteRegisterDialog::WriteRegisterDialog(const ChannelModel *channelModel, QWidget *parent)
    : QDialog(parent)
    , m_channelModel(channelModel)
{
    initUi();
    initConnections();
    refreshModeState();
}

WriteRegisterDialog::~WriteRegisterDialog() = default;

void WriteRegisterDialog::initUi()
{
    setWindowTitle(LANGUAGE("LG_write_title"));
    setModal(true);

    m_protectCheck = new QCheckBox(LANGUAGE("LG_write_protect"), this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(LANGUAGE("LG_write_mode_singleReg"), ModeSingleRegister);
    m_modeCombo->addItem(LANGUAGE("LG_write_mode_multiReg"), ModeMultipleRegisters);
    m_modeCombo->addItem(LANGUAGE("LG_write_mode_singleCoil"), ModeSingleCoil);
    m_modeCombo->addItem(LANGUAGE("LG_write_mode_multiCoil"), ModeMultipleCoils);

    m_slaveSpin = new QSpinBox(this);
    m_slaveSpin->setRange(1, 247);

    m_addressSpin = new QSpinBox(this);
    m_addressSpin->setRange(0, 65535);
    m_addressSpin->setDisplayIntegerBase(10);

    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setRange(1, 123);

    m_valueEdit = new QLineEdit(QStringLiteral("0x0000"), this);
    m_tipLabel = new QLabel(LANGUAGE("LG_write_valueTip"), this);

    // 默认取第一个启用通道的从站与地址，减少手工输入
    if (m_channelModel != nullptr) {
        for (int index = 0; index < m_channelModel->channelCount(); ++index) {
            const ChannelItem &item = m_channelModel->channelAt(index);
            if (item.enabled) {
                m_slaveSpin->setValue(item.slaveId);
                m_addressSpin->setValue(item.registerAddress);
                break;
            }
        }
    }

    auto *form = new QFormLayout;
    form->addRow(LANGUAGE("LG_write_mode"), m_modeCombo);
    form->addRow(LANGUAGE("LG_write_slave"), m_slaveSpin);
    form->addRow(LANGUAGE("LG_write_address"), m_addressSpin);
    form->addRow(LANGUAGE("LG_write_quantity"), m_quantitySpin);
    form->addRow(LANGUAGE("LG_write_value"), m_valueEdit);
    form->addRow(QString(), m_tipLabel);

    m_writeButton = new QPushButton(LANGUAGE("LG_write_btn"), this);
    m_writeButton->setDefault(true);
    m_cancelButton = new QPushButton(LANGUAGE("LG_cancel"), this);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_writeButton);
    buttons->addWidget(m_cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_protectCheck);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void WriteRegisterDialog::initConnections()
{
    connect(m_modeCombo, &QComboBox::currentIndexChanged,
            this, &WriteRegisterDialog::onModeChanged);
    connect(m_protectCheck, &QCheckBox::toggled,
            this, &WriteRegisterDialog::onProtectToggled);
    connect(m_writeButton, &QPushButton::clicked, this, &WriteRegisterDialog::submitWrite);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void WriteRegisterDialog::onModeChanged(int index)
{
    Q_UNUSED(index)
    refreshModeState();
}

void WriteRegisterDialog::onProtectToggled(bool checked)
{
    m_writeButton->setEnabled(checked);
}

void WriteRegisterDialog::refreshModeState()
{
    const int mode = m_modeCombo->currentData().toInt();
    const bool multiple = (mode == ModeMultipleRegisters || mode == ModeMultipleCoils);
    const bool coil = (mode == ModeSingleCoil || mode == ModeMultipleCoils);

    m_quantitySpin->setEnabled(multiple);
    m_quantitySpin->setRange(1, coil ? 1968 : 123);
    if (!multiple) {
        m_quantitySpin->setValue(1);
    }

    if (coil) {
        m_valueEdit->setPlaceholderText(QStringLiteral("1, 0, 1"));
        m_tipLabel->setText(LANGUAGE("LG_write_coilValueTip"));
    } else {
        m_valueEdit->setPlaceholderText(QStringLiteral("0x0064, 0x00C8"));
        m_tipLabel->setText(LANGUAGE("LG_write_valueTip"));
    }

    m_writeButton->setEnabled(m_protectCheck->isChecked());
}

bool WriteRegisterDialog::parseValues(QVector<quint16> *values) const
{
    const QStringList tokens = m_valueEdit->text().split(
        QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return false;
    }

    for (const QString &token : tokens) {
        bool ok = false;
        const uint value = token.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
                               ? token.mid(2).toUInt(&ok, 16)
                               : token.toUInt(&ok, 10);
        if (!ok || value > 0xFFFF) {
            return false;
        }
        values->append(static_cast<quint16>(value));
    }
    return true;
}

bool WriteRegisterDialog::parseCoils(QVector<bool> *coils) const
{
    QVector<quint16> values;
    if (!parseValues(&values)) {
        return false;
    }
    for (quint16 value : values) {
        if (value > 1) {
            return false;
        }
        coils->append(value == 1);
    }
    return true;
}

bool WriteRegisterDialog::confirmWrite(quint16 quantity)
{
    const QString text = LANGUAGE("LG_write_confirmFmt")
                             .arg(m_slaveSpin->value())
                             .arg(m_addressSpin->value())
                             .arg(quantity);
    return QMessageBox::question(this,
                                 LANGUAGE("LG_write_confirmTitle"), text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

void WriteRegisterDialog::submitWrite()
{
    if (!m_protectCheck->isChecked()) {
        QMessageBox::warning(this, LANGUAGE("LG_write_title"), LANGUAGE("LG_write_needProtect"));
        return;
    }

    const int mode = m_modeCombo->currentData().toInt();
    const quint8 slaveId = static_cast<quint8>(m_slaveSpin->value());
    const quint16 address = static_cast<quint16>(m_addressSpin->value());

    if (mode == ModeSingleRegister || mode == ModeMultipleRegisters) {
        QVector<quint16> values;
        if (!parseValues(&values)) {
            QMessageBox::warning(this, LANGUAGE("LG_write_title"),
                                 LANGUAGE("LG_write_invalidValue"));
            return;
        }
        const quint16 quantity = static_cast<quint16>(values.size());
        if (mode == ModeSingleRegister && quantity != 1) {
            QMessageBox::warning(this, LANGUAGE("LG_write_title"),
                                 LANGUAGE("LG_write_invalidValue"));
            return;
        }
        if (!confirmWrite(quantity)) {
            return;
        }
        emit writeRegistersRequested(slaveId, address, values);
        setWriteInFlight(true);
        return;
    }

    QVector<bool> coils;
    if (!parseCoils(&coils)) {
        QMessageBox::warning(this, LANGUAGE("LG_write_title"), LANGUAGE("LG_write_invalidValue"));
        return;
    }
    if (mode == ModeSingleCoil) {
        if (coils.size() != 1 || !confirmWrite(1)) {
            return;
        }
        emit writeSingleCoilRequested(slaveId, address, coils.first());
        setWriteInFlight(true);
        return;
    }

    if (!confirmWrite(static_cast<quint16>(coils.size()))) {
        return;
    }
    emit writeCoilsRequested(slaveId, address, coils);
    setWriteInFlight(true);
}

void WriteRegisterDialog::setWriteInFlight(bool inFlight)
{
    m_writeInFlight = inFlight;
    m_writeButton->setEnabled(!inFlight && m_protectCheck->isChecked());
    m_modeCombo->setEnabled(!inFlight);
    m_slaveSpin->setEnabled(!inFlight);
    m_addressSpin->setEnabled(!inFlight);
    m_quantitySpin->setEnabled(!inFlight);
    m_valueEdit->setEnabled(!inFlight);
    m_protectCheck->setEnabled(!inFlight);
}

void WriteRegisterDialog::onWriteSucceeded(quint8 functionCode, quint16 startAddress,
                                           quint16 quantity)
{
    Q_UNUSED(functionCode)
    Q_UNUSED(startAddress)
    Q_UNUSED(quantity)

    // 成功：关闭对话框；状态栏提示由 MainWidget 统一处理
    m_writeInFlight = false;
    accept();
}

void WriteRegisterDialog::onWriteFailed(ModbusError error, const QString &message)
{
    Q_UNUSED(error)

    // 失败：保持对话框打开，恢复输入并提示，便于就地重试
    m_writeInFlight = false;
    setWriteInFlight(false);
    QMessageBox::warning(this, LANGUAGE("LG_write_title"),
                         LANGUAGE("LG_write_failedFmt").arg(message));
}
