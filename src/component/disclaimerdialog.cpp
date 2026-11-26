#include "disclaimerdialog.h"
#include "Language.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

DisclaimerDialog::DisclaimerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(LANGUAGE("LG_disclaimer_title"));
    setModal(true);
    setMinimumWidth(560);
    initUi();
    initConnections();
}

void DisclaimerDialog::initUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    QLabel *titleLabel = new QLabel(LANGUAGE("LG_disclaimer_title"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(LANGUAGE("LG_disclaimer_subtitle"), this);
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    m_bodyLabel = new QLabel(buildBodyText(), this);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_bodyLabel, 1);

    QLabel *hintLabel = new QLabel(LANGUAGE("LG_disclaimer_hint"), this);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    m_acceptButton = new QPushButton(LANGUAGE("LG_disclaimer_accept"), this);
    m_acceptButton->setDefault(true);
    m_declineButton = new QPushButton(LANGUAGE("LG_disclaimer_decline"), this);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_acceptButton);
    buttonLayout->addWidget(m_declineButton);
    layout->addLayout(buttonLayout);
}

void DisclaimerDialog::initConnections()
{
    connect(m_acceptButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_declineButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString DisclaimerDialog::buildBodyText() const
{
    QStringList lines;
    lines << LANGUAGE("LG_disclaimer_line1")
          << LANGUAGE("LG_disclaimer_line2")
          << LANGUAGE("LG_disclaimer_line3")
          << LANGUAGE("LG_disclaimer_line4")
          << LANGUAGE("LG_disclaimer_line5");
    return lines.join(QStringLiteral("\n\n"));
}
