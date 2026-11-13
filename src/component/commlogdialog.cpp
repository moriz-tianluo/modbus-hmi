#include "commlogdialog.h"
#include "Language.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {
const int kMaxLogLines = 2000;  // 超出后自动丢弃最早的行，避免内存无限增长
}

CommLogDialog::CommLogDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(LANGUAGE("LG_commlog_title"));
    resize(720, 520);

    buildUi();
}

// ============== 界面搭建 ==============

void CommLogDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logEdit->document()->setMaximumBlockCount(kMaxLogLines);
    // 等宽字体，保证十六进制帧对齐
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(9);
    m_logEdit->setFont(monoFont);
    layout->addWidget(m_logEdit, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout;

    m_pauseCheck = new QCheckBox(LANGUAGE("LG_commlog_pause"), this);
    QPushButton *clearButton = new QPushButton(LANGUAGE("LG_commlog_btnClear"), this);
    QPushButton *exportButton = new QPushButton(LANGUAGE("LG_commlog_btnExport"), this);
    QPushButton *closeButton = new QPushButton(LANGUAGE("LG_commlog_btnClose"), this);

    connect(clearButton, &QPushButton::clicked, this, &CommLogDialog::onClearClicked);
    connect(exportButton, &QPushButton::clicked, this, &CommLogDialog::onExportClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(m_pauseCheck);
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

// ============== 日志追加 ==============

void CommLogDialog::onFrameLogged(bool outgoing, const QByteArray &frame,
                                  qint64 timestampMs)
{
    if (m_pauseCheck->isChecked() || frame.isEmpty()) {
        return;
    }

    const QString timeText = QDateTime::fromMSecsSinceEpoch(timestampMs)
                                 .toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString direction = outgoing ? LANGUAGE("LG_commlog_tx")
                                       : LANGUAGE("LG_commlog_rx");
    m_logEdit->appendPlainText(
        QStringLiteral("[%1] %2  %3").arg(timeText, direction, frameToHex(frame)));
}

QString CommLogDialog::frameToHex(const QByteArray &frame) const
{
    QString text = QString::fromLatin1(frame.toHex(' ').toUpper());
    return text;
}

// ============== 清空 / 导出 ==============

void CommLogDialog::onClearClicked()
{
    m_logEdit->clear();
}

void CommLogDialog::onExportClicked()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, LANGUAGE("LG_commlog_exportTitle"), QString(),
        LANGUAGE("LG_commlog_exportFilter"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, LANGUAGE("LG_commlog_title"),
                             LANGUAGE("LG_commlog_exportFailed"));
        return;
    }

    file.write(m_logEdit->toPlainText().toUtf8());
    file.close();

    QMessageBox::information(this, LANGUAGE("LG_commlog_title"),
                             LANGUAGE("LG_commlog_exportDoneFmt").arg(filePath));
}
