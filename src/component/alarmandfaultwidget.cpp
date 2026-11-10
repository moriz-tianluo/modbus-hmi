#include "alarmandfaultwidget.h"
#include "ui_alarmandfaultwidget.h"
#include "Language.h"

AlarmAndFaultWidget::AlarmAndFaultWidget(QWidget *parent)
    : BaseWidget(parent)
    , ui(new Ui::AlarmAndFaultWidget)
{
    ui->setupUi(this);
    // 自定义 QWidget 子类需开启该属性，样式表中的背景与边框才会生效
    setAttribute(Qt::WA_StyledBackground, true);
    languageChange();
}

AlarmAndFaultWidget::~AlarmAndFaultWidget()
{
    delete ui;
}

void AlarmAndFaultWidget::languageChange()
{
    ui->labelTitle->setText(LANGUAGE("LG_card_alarmFault"));

    // 条目标签：报警 / 故障
    ui->labelAlarmTag1->setText(LANGUAGE("LG_alarm_tag_alarm"));
    ui->labelAlarmTag2->setText(LANGUAGE("LG_alarm_tag_fault"));
    ui->labelAlarmTag3->setText(LANGUAGE("LG_alarm_tag_fault"));

    // 条目描述
    ui->labelAlarmMsg1->setText(LANGUAGE("LG_alarm_msg_tempOver"));
    ui->labelAlarmMsg2->setText(LANGUAGE("LG_alarm_msg_flowTimeout"));
    ui->labelAlarmMsg3->setText(LANGUAGE("LG_alarm_msg_serialReconnect"));

    // 底部说明与按钮
    ui->labelFilterTip->setText(LANGUAGE("LG_alarm_filterTip"));
    ui->labelParamTip->setText(LANGUAGE("LG_alarm_paramTip"));
    ui->pushButtonViewAll->setText(LANGUAGE("LG_alarm_btn_all"));
    ui->pushButtonClear->setText(LANGUAGE("LG_alarm_btn_clear"));
    ui->labelTotalCount->setText(LANGUAGE("LG_alarm_totalCount"));
}
