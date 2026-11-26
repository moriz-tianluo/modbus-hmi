#ifndef DISCLAIMERDIALOG_H
#define DISCLAIMERDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

// 启动免责声明对话框：说明免费发布与免责条款，每次启动都会展示。
// “同意”后进入主界面；拒绝则直接退出程序。
class DisclaimerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisclaimerDialog(QWidget *parent = nullptr);

private:
    // 搭建标题、正文、提示与底部按钮
    void initUi();
    void initConnections();
    // 组装多行声明正文，文本统一来自 Language
    QString buildBodyText() const;

private:
    QLabel *m_bodyLabel = nullptr;
    QPushButton *m_acceptButton = nullptr;
    QPushButton *m_declineButton = nullptr;
};

#endif // DISCLAIMERDIALOG_H
