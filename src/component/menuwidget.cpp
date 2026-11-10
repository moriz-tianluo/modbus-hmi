#include "menuwidget.h"
#include "ui_menuwidget.h"
#include "Language.h"

menuWidget::menuWidget(QWidget *parent)
    : BaseWidget(parent)    
    , ui(new Ui::menuWidget)
{
    ui->setupUi(this);
    languageChange();
}

menuWidget::~menuWidget()
{
    delete ui;
}

void menuWidget::languageChange()
{
    ui->toolButtonFile->setText(LANGUAGE("LG_menu_file"));
    ui->toolButtonEdit->setText(LANGUAGE("LG_menu_edit"));
    ui->toolButtonDevice->setText(LANGUAGE("LG_menu_device"));
    ui->toolButtonView->setText(LANGUAGE("LG_menu_view"));
    ui->toolButtonTool->setText(LANGUAGE("LG_menu_tools"));
    ui->toolButtonHelp->setText(LANGUAGE("LG_menu_help"));
}
