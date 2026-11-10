#ifndef CHANNELSETWIDGET_H
#define CHANNELSETWIDGET_H

#include "base/BaseWidget.h"

namespace Ui {
class ChannelSetWidget;
}

class ChannelSetWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit ChannelSetWidget(QWidget *parent = nullptr);
    ~ChannelSetWidget();

    void languageChange() override;

private:
    // 初始化通道表格的列宽与表头高度
    void initTable();

    // 设置指列表头文字
    void setHeaderText(int column, const QString &text);

private:
    Ui::ChannelSetWidget *ui;
};

#endif // CHANNELSETWIDGET_H
