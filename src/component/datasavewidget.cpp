#include "datasavewidget.h"
#include "ui_datasavewidget.h"
#include "Language.h"
#include "common/savedatasql.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

DataSaveWidget::DataSaveWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::DataSaveWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);

    connect(ui->pushButtonQuery, &QPushButton::clicked, this, &DataSaveWidget::onQueryClicked);
    connect(ui->pushButtonExport, &QPushButton::clicked, this, &DataSaveWidget::onExportClicked);
    connect(ui->comboBoxTimeRange, &QComboBox::currentIndexChanged,
            this, &DataSaveWidget::onTimeRangeChanged);

    languageChange();
}

DataSaveWidget::~DataSaveWidget()
{
    delete ui;
}

void DataSaveWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_dataStore"));
    ui->pushButtonQuery->setText(LANGUAGE("LG_store_btn_query"));
    ui->pushButtonExport->setText(LANGUAGE("LG_store_btn_export"));

    initTimeRangeItems();
    refreshRecordCountText();
    refreshDatabaseInfo();
}

void DataSaveWidget::initTimeRangeItems()
{
    const int currentIndex = ui->comboBoxTimeRange->currentIndex();

    {
        QSignalBlocker blocker(ui->comboBoxTimeRange);
        ui->comboBoxTimeRange->clear();
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_1h"));
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_6h"));
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_24h"));
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_7d"));
        ui->comboBoxTimeRange->addItem(LANGUAGE("LG_store_range_custom"));
        ui->comboBoxTimeRange->setCurrentIndex(currentIndex < 0 ? 0 : currentIndex);
    }
}

QPair<qint64, qint64> DataSaveWidget::currentTimeRange() const
{
    // 自定义范围直接返回用户设置的时间
    if (ui->comboBoxTimeRange->currentIndex() == kCustomRangeIndex
        && m_customToMs > m_customFromMs) {
        return qMakePair(m_customFromMs, m_customToMs);
    }

    // 其余下拉项与小时数一一对应：1h / 6h / 24h / 7 天
    qint64 hours = 1;
    switch (ui->comboBoxTimeRange->currentIndex()) {
    case 0:
        hours = 1;
        break;
    case 1:
        hours = 6;
        break;
    case 2:
        hours = 24;
        break;
    case 3:
        hours = 24 * 7;
        break;
    default:
        hours = 1;
        break;
    }

    const qint64 toMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 fromMs = toMs - hours * 3600LL * 1000LL;
    return qMakePair(fromMs, toMs);
}

void DataSaveWidget::onTimeRangeChanged(int index)
{
    if (index != kCustomRangeIndex) {
        return;
    }

    // 取消选择自定义范围时回退到最近 1 小时
    if (!askCustomRange()) {
        QSignalBlocker blocker(ui->comboBoxTimeRange);
        ui->comboBoxTimeRange->setCurrentIndex(0);
    }
}

bool DataSaveWidget::askCustomRange()
{
    QDialog dialog(this);
    dialog.setWindowTitle(LANGUAGE("LG_store_customTitle"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QDateTimeEdit *fromEdit = new QDateTimeEdit(&dialog);
    QDateTimeEdit *toEdit = new QDateTimeEdit(&dialog);
    QDateTimeEdit *edits[] = { fromEdit, toEdit };
    for (QDateTimeEdit *edit : edits) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }

    fromEdit->setDateTime(m_customToMs > m_customFromMs
                              ? QDateTime::fromMSecsSinceEpoch(m_customFromMs)
                              : QDateTime::currentDateTime().addSecs(-3600));
    toEdit->setDateTime(m_customToMs > m_customFromMs
                            ? QDateTime::fromMSecsSinceEpoch(m_customToMs)
                            : QDateTime::currentDateTime());

    layout->addWidget(new QLabel(LANGUAGE("LG_history_from"), &dialog));
    layout->addWidget(fromEdit);
    layout->addWidget(new QLabel(LANGUAGE("LG_history_to"), &dialog));
    layout->addWidget(toEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(LANGUAGE("LG_history_btnQuery"));
    buttons->button(QDialogButtonBox::Cancel)->setText(LANGUAGE("LG_channel_btnClose"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const qint64 fromMs = fromEdit->dateTime().toMSecsSinceEpoch();
    const qint64 toMs = toEdit->dateTime().toMSecsSinceEpoch();
    m_customFromMs = qMin(fromMs, toMs);
    m_customToMs = qMax(fromMs, toMs);
    return true;
}

void DataSaveWidget::onQueryClicked()
{
    const QPair<qint64, qint64> range = currentTimeRange();
    emit recordCountRequested(range.first, range.second);
    // 同步打开历史查询对话框，展示可浏览的结果表格
    emit historyQueryRequested(range.first, range.second);
}

void DataSaveWidget::requestExport()
{
    onExportClicked();
}

void DataSaveWidget::onExportClicked()
{
    const QPair<qint64, qint64> range = currentTimeRange();

    // 先让用户选择导出列，取消则放弃本次导出
    int fieldMask = 0;
    if (!askExportFields(&fieldMask)) {
        return;
    }

    // 默认导出到程序目录，文件名带时间戳，避免覆盖
    const QString defaultName = QDir(QCoreApplication::applicationDirPath())
                                    .filePath(QStringLiteral("hmi_%1.csv").arg(
                                        QDateTime::currentDateTime().toString(
                                            QStringLiteral("yyyyMMdd_HHmmss"))));
    const QString filePath = QFileDialog::getSaveFileName(
        this, LANGUAGE("LG_store_exportTitle"), defaultName, LANGUAGE("LG_store_exportFilter"));
    if (filePath.isEmpty()) {
        return;
    }

    emit exportRequested(range.first, range.second, filePath, fieldMask);
}

bool DataSaveWidget::askExportFields(int *mask)
{
    if (mask == nullptr) {
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(LANGUAGE("LG_store_exportFieldsTitle"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(LANGUAGE("LG_store_exportFieldsTip"), &dialog));

    QCheckBox *timeCheck = new QCheckBox(LANGUAGE("LG_store_fieldTime"), &dialog);
    QCheckBox *channelCheck = new QCheckBox(LANGUAGE("LG_store_fieldChannel"), &dialog);
    QCheckBox *rawCheck = new QCheckBox(LANGUAGE("LG_store_fieldRaw"), &dialog);
    QCheckBox *valueCheck = new QCheckBox(LANGUAGE("LG_store_fieldValue"), &dialog);
    QCheckBox *checks[] = { timeCheck, channelCheck, rawCheck, valueCheck };
    for (QCheckBox *check : checks) {
        check->setChecked(true);
        layout->addWidget(check);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(LANGUAGE("LG_store_btnExport"));
    buttons->button(QDialogButtonBox::Cancel)->setText(LANGUAGE("LG_channel_btnClose"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    int result = 0;
    if (timeCheck->isChecked()) {
        result |= SaveDataSQL::ExportTime;
    }
    if (channelCheck->isChecked()) {
        result |= SaveDataSQL::ExportChannel;
    }
    if (rawCheck->isChecked()) {
        result |= SaveDataSQL::ExportRaw;
    }
    if (valueCheck->isChecked()) {
        result |= SaveDataSQL::ExportValue;
    }

    *mask = result;
    return true;
}

void DataSaveWidget::onRecordCountReady(qint64 fromMs, qint64 toMs, qint64 count)
{
    Q_UNUSED(fromMs)
    Q_UNUSED(toMs)

    m_lastCount = count;
    refreshRecordCountText();
    // 查询发生在数据库线程，此时库文件已就绪，顺带刷新文件大小信息
    refreshDatabaseInfo();
}

void DataSaveWidget::onExportFinished(bool ok, qint64 rows, const QString &filePath,
                                      const QString &errorString)
{
    if (ok) {
        QMessageBox::information(this, LANGUAGE("LG_store_msgTitle"),
                                 LANGUAGE("LG_store_exportDoneFmt").arg(rows).arg(filePath));
        return;
    }

    const QString reason = errorString.isEmpty()
                               ? LANGUAGE("LG_store_err_dbNotReady")
                               : errorString;
    QMessageBox::warning(this, LANGUAGE("LG_store_msgTitle"),
                         LANGUAGE("LG_store_exportFailFmt").arg(reason));
}

void DataSaveWidget::refreshRecordCountText()
{
    ui->labelRecordCount->setText(LANGUAGE("LG_store_countFmt").arg(m_lastCount));
}

void DataSaveWidget::refreshDatabaseInfo()
{
    const QFileInfo info(QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("data/hmi.db")));
    const double sizeMb = info.exists() ? info.size() / (1024.0 * 1024.0) : 0.0;
    const QString fileName = info.fileName().isEmpty() ? QStringLiteral("hmi.db") : info.fileName();

    ui->labelDbInfo->setText(LANGUAGE("LG_store_dbInfoFmt")
                                 .arg(fileName)
                                 .arg(QString::number(sizeMb, 'f', 2)));
}
