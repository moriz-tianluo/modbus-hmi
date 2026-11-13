#ifndef COMMLOGDIALOG_H
#define COMMLOGDIALOG_H

#include <QDialog>

class QCheckBox;
class QPlainTextEdit;

// 通信日志窗口：
//   - 显示串口收发的十六进制帧，带本地时间戳
//   - 支持暂停刷新、清空、导出为文本
class CommLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommLogDialog(QWidget *parent = nullptr);

public slots:
    // 接收 SerialWorker 透传的收发帧
    void onFrameLogged(bool outgoing, const QByteArray &frame, qint64 timestampMs);

private slots:
    void onClearClicked();
    void onExportClicked();

private:
    // 搭建日志区与按钮
    void buildUi();
    // 帧转十六进制文本
    QString frameToHex(const QByteArray &frame) const;

private:
    QPlainTextEdit *m_logEdit = nullptr;
    QCheckBox *m_pauseCheck = nullptr;
};

#endif // COMMLOGDIALOG_H
