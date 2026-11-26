#ifndef WRITEREGISTERDIALOG_H
#define WRITEREGISTERDIALOG_H

#include "channelmodel.h"
#include "modbusrtu.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

// 写寄存器 / 线圈对话框（05/06/15/16）。
// 写保护：必须勾选「启用写操作」后才能点击写入；点击写入时再弹一次确认框。
class WriteRegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WriteRegisterDialog(const ChannelModel *channelModel, QWidget *parent = nullptr);
    ~WriteRegisterDialog() override;

public slots:
    // 写操作结果（由 SerialWorker 跨线程回传）：成功关闭对话框，失败保留以便重试
    void onWriteSucceeded(quint8 functionCode, quint16 startAddress, quint16 quantity);
    void onWriteFailed(ModbusError error, const QString &message);
    // 写入进行中禁止取消 / Esc 关闭，避免结果丢失
    void reject() override;

signals:
    // 06 / 16：从站 + 起始地址 + 寄存器值列表
    void writeRegistersRequested(quint8 slaveId, quint16 address, const QVector<quint16> &values);
    // 05：单个线圈
    void writeSingleCoilRequested(quint8 slaveId, quint16 address, bool on);
    // 15：多个线圈
    void writeCoilsRequested(quint8 slaveId, quint16 address, const QVector<bool> &values);

protected:
    // 写入进行中拦截窗口关闭（标题栏 X / Alt+F4）
    void closeEvent(QCloseEvent *event) override;

private:
    void initUi();
    void initConnections();
    void refreshModeState();
    // 解析输入框中的数值列表，失败返回 false
    bool parseValues(QVector<quint16> *values) const;
    // 解析线圈列表（每个值只能是 0/1），失败返回 false
    bool parseCoils(QVector<bool> *values) const;
    // 校验写保护、连接状态与输入，通过后发出对应信号
    void submitWrite();
    bool confirmWrite(quint16 quantity);
    // 写入进行中禁用输入控件，避免重复提交或中途改值
    void setWriteInFlight(bool inFlight);
    // 多值模式下让「数量」与值列表个数一致，避免数量输入与下发内容不符
    void syncQuantityFromValues();

private slots:
    void onModeChanged(int index);
    void onProtectToggled(bool checked);
    // 值列表变化时，多寄存器 / 多线圈模式的「数量」跟随实际个数
    void onValueTextChanged(const QString &text);

private:
    const ChannelModel *m_channelModel = nullptr;
    bool m_writeInFlight = false;

    QCheckBox *m_protectCheck = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_slaveSpin = nullptr;
    QSpinBox *m_addressSpin = nullptr;
    QSpinBox *m_quantitySpin = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QLabel *m_tipLabel = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};

#endif // WRITEREGISTERDIALOG_H
