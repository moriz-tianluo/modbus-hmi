#ifndef WRITEREGISTERDIALOG_H
#define WRITEREGISTERDIALOG_H

#include "channelmodel.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
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

signals:
    // 06 / 16：从站 + 起始地址 + 寄存器值列表
    void writeRegistersRequested(quint8 slaveId, quint16 address, const QVector<quint16> &values);
    // 05：单个线圈
    void writeSingleCoilRequested(quint8 slaveId, quint16 address, bool on);
    // 15：多个线圈
    void writeCoilsRequested(quint8 slaveId, quint16 address, const QVector<bool> &values);

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

private slots:
    void onModeChanged(int index);
    void onProtectToggled(bool checked);

private:
    const ChannelModel *m_channelModel = nullptr;

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
