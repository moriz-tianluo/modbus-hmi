#include "historyquerydialog.h"
#include "Language.h"
#include "common/channelmodel.h"

#include "qcustomplot.h"

#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

HistoryQueryDialog::HistoryQueryDialog(ChannelModel *model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(LANGUAGE("LG_history_title"));
    resize(900, 620);

    buildUi();
    refreshChannelCombo();

    if (m_model != nullptr) {
        connect(m_model, &ChannelModel::channelsChanged,
                this, &HistoryQueryDialog::onChannelsChanged);
    }
}

// ============== 界面搭建 ==============

void HistoryQueryDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    buildFilterBar(layout);

    // ---- 结果表格 ----
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        LANGUAGE("LG_history_col_time"),
        LANGUAGE("LG_history_col_channel"),
        LANGUAGE("LG_history_col_raw"),
        LANGUAGE("LG_history_col_value"),
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 190);
    m_table->setColumnWidth(1, 130);
    m_table->setColumnWidth(2, 100);
    layout->addWidget(m_table, 2);

    // ---- 趋势图 ----
    buildPlot(layout);

    // ---- 关闭 ----
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    QPushButton *closeButton = new QPushButton(LANGUAGE("LG_history_btnClose"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void HistoryQueryDialog::buildFilterBar(QVBoxLayout *layout)
{
    QHBoxLayout *filterLayout = new QHBoxLayout;

    m_fromEdit = new QDateTimeEdit(this);
    m_fromEdit->setCalendarPopup(true);
    m_fromEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_fromEdit->setDateTime(QDateTime::currentDateTime().addSecs(-3600));

    m_toEdit = new QDateTimeEdit(this);
    m_toEdit->setCalendarPopup(true);
    m_toEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_toEdit->setDateTime(QDateTime::currentDateTime());

    m_channelCombo = new QComboBox(this);

    QPushButton *queryButton = new QPushButton(LANGUAGE("LG_history_btnQuery"), this);
    connect(queryButton, &QPushButton::clicked,
            this, &HistoryQueryDialog::onQueryClicked);

    m_countLabel = new QLabel(LANGUAGE("LG_history_countFmt").arg(0), this);

    filterLayout->addWidget(new QLabel(LANGUAGE("LG_history_from"), this));
    filterLayout->addWidget(m_fromEdit);
    filterLayout->addWidget(new QLabel(LANGUAGE("LG_history_to"), this));
    filterLayout->addWidget(m_toEdit);
    filterLayout->addWidget(new QLabel(LANGUAGE("LG_rd_col_channel"), this));
    filterLayout->addWidget(m_channelCombo);
    filterLayout->addWidget(queryButton);
    filterLayout->addWidget(m_countLabel);
    filterLayout->addStretch();

    layout->addLayout(filterLayout);
}

void HistoryQueryDialog::buildPlot(QVBoxLayout *layout)
{
    m_plot = new QCustomPlot(this);
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(QColor(0x1F, 0x6F, 0xEB), 1.6));

    QSharedPointer<QCPAxisTickerDateTime> ticker(new QCPAxisTickerDateTime);
    ticker->setDateTimeFormat(QStringLiteral("HH:mm:ss"));
    m_plot->xAxis->setTicker(ticker);
    m_plot->xAxis->setTickLabelFont(QFont(QString(), 8));
    m_plot->yAxis->setTickLabelFont(QFont(QString(), 8));
    m_plot->xAxis->grid()->setPen(QPen(QColor(0xE9, 0xED, 0xF5), 1, Qt::DashLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(0xE9, 0xED, 0xF5), 1, Qt::DashLine));
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    layout->addWidget(m_plot, 3);
}

// ============== 查询流程 ==============

void HistoryQueryDialog::openWithRange(qint64 fromMs, qint64 toMs, int channelIndex)
{
    m_fromEdit->setDateTime(QDateTime::fromMSecsSinceEpoch(fromMs));
    m_toEdit->setDateTime(QDateTime::fromMSecsSinceEpoch(toMs));

    // 下拉第 0 项为全部通道，通道下标 +1
    const int comboIndex = m_channelCombo->findData(channelIndex);
    m_channelCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);

    show();
    raise();
    activateWindow();
    onQueryClicked();
}

void HistoryQueryDialog::onQueryClicked()
{
    const qint64 fromMs = m_fromEdit->dateTime().toMSecsSinceEpoch();
    const qint64 toMs = m_toEdit->dateTime().toMSecsSinceEpoch();
    const int channelIndex = m_channelCombo->currentData().toInt();

    emit queryRequested(qMin(fromMs, toMs), qMax(fromMs, toMs), channelIndex);
}

void HistoryQueryDialog::onSamplesQueried(qint64 fromMs, qint64 toMs,
                                          const QVector<SampleChannelData> &rows)
{
    Q_UNUSED(fromMs)
    Q_UNUSED(toMs)

    fillTable(rows);
    fillPlot(rows);

    // 达到单次查询上限时明确提示，避免用户误以为数据缺失
    if (rows.size() >= kQueryLimit) {
        m_countLabel->setText(LANGUAGE("LG_history_truncatedFmt").arg(kQueryLimit));
        m_countLabel->setStyleSheet(QStringLiteral("color: #E0483F;"));
    } else {
        m_countLabel->setText(LANGUAGE("LG_history_countFmt").arg(rows.size()));
        m_countLabel->setStyleSheet(QString());
    }
}

// ============== 结果展示 ==============

void HistoryQueryDialog::fillTable(const QVector<SampleChannelData> &rows)
{
    m_table->setRowCount(0);
    m_table->setRowCount(rows.size());

    for (int row = 0; row < rows.size(); ++row) {
        const SampleChannelData &data = rows.at(row);
        const QString timeText = QDateTime::fromMSecsSinceEpoch(data.ts)
                                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

        m_table->setItem(row, 0, new QTableWidgetItem(timeText));
        m_table->setItem(row, 1, new QTableWidgetItem(
            channelName(data.channel)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(data.raw)));
        m_table->setItem(row, 3, new QTableWidgetItem(
            QString::number(data.value, 'f', 3)));
    }
}

void HistoryQueryDialog::fillPlot(const QVector<SampleChannelData> &rows)
{
    if (m_plot == nullptr || m_plot->graphCount() == 0) {
        return;
    }

    QVector<double> keys;
    QVector<double> values;
    keys.reserve(rows.size());
    values.reserve(rows.size());

    for (const SampleChannelData &data : rows) {
        keys.append(static_cast<double>(data.ts) / 1000.0);
        values.append(data.value);
    }

    m_plot->graph(0)->setData(keys, values, true);

    if (!keys.isEmpty()) {
        const double padding = qMax(1.0, (keys.last() - keys.first()) * 0.02);
        m_plot->xAxis->setRange(keys.first() - padding, keys.last() + padding);
        m_plot->yAxis->rescale();
        m_plot->yAxis->scaleRange(1.1, m_plot->yAxis->range().center());
    }

    m_plot->replot();
}

void HistoryQueryDialog::refreshChannelCombo()
{
    const int currentChannel = m_channelCombo->currentData().toInt();

    QSignalBlocker blocker(m_channelCombo);
    m_channelCombo->clear();
    m_channelCombo->addItem(LANGUAGE("LG_history_channelAll"), -1);

    if (m_model != nullptr) {
        for (int index = 0; index < m_model->channelCount(); ++index) {
            m_channelCombo->addItem(m_model->channelAt(index).name, index);
        }
    }

    const int comboIndex = m_channelCombo->findData(currentChannel);
    m_channelCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
}

void HistoryQueryDialog::onChannelsChanged()
{
    refreshChannelCombo();
}

QString HistoryQueryDialog::channelName(int channel) const
{
    if (m_model != nullptr && channel >= 0 && channel < m_model->channelCount()) {
        return m_model->channelAt(channel).name;
    }
    return QString::number(channel);
}
