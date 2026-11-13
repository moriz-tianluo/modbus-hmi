#include "curvesettingsdialog.h"
#include "Language.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CurveSettingsDialog::CurveSettingsDialog(int lineWidth, bool limitVisible, double limitValue,
                                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(LANGUAGE("LG_curve_settingsTitle"));
    resize(320, 0);

    buildUi();

    m_widthSpin->setValue(qBound(1, lineWidth, 6));
    m_limitCheck->setChecked(limitVisible);
    m_limitSpin->setValue(qBound(0.0, limitValue, 100.0));
}

int CurveSettingsDialog::lineWidth() const
{
    return m_widthSpin->value();
}

bool CurveSettingsDialog::upperLimitVisible() const
{
    return m_limitCheck->isChecked();
}

double CurveSettingsDialog::upperLimitValue() const
{
    return m_limitSpin->value();
}

void CurveSettingsDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QFormLayout *form = new QFormLayout;

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 6);
    form->addRow(LANGUAGE("LG_curve_lineWidth"), m_widthSpin);

    m_limitCheck = new QCheckBox(LANGUAGE("LG_curve_showUpperLimit"), this);
    form->addRow(QString(), m_limitCheck);

    m_limitSpin = new QDoubleSpinBox(this);
    m_limitSpin->setRange(0.0, 100.0);
    m_limitSpin->setDecimals(0);
    m_limitSpin->setSuffix(QStringLiteral(" %"));
    form->addRow(LANGUAGE("LG_curve_upperLimitValue"), m_limitSpin);

    layout->addLayout(form);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(LANGUAGE("LG_channel_btnApply"));
    buttons->button(QDialogButtonBox::Cancel)->setText(LANGUAGE("LG_channel_btnClose"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
