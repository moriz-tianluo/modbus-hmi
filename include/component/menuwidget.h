#ifndef MENUWIDGET_H
#define MENUWIDGET_H

#include "base/BaseWidget.h"


namespace Ui {
class menuWidget;
}   

class menuWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit menuWidget(QWidget *parent = nullptr);
    ~menuWidget();

    void languageChange() override;

private:
    Ui::menuWidget *ui;
};

#endif // MENUWIDGET_H
