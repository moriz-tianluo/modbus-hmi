#include "BaseWidget.h"
#include <QEvent>

BaseWidget::BaseWidget(QWidget *parent)
    : QWidget(parent)
    , showTimer(nullptr)
    , bShowAfter(true)
    , bFirstShow(true)
{
}

BaseWidget::~BaseWidget()
{
}

//初始化显示定时器
void BaseWidget::initShowTimer()
{
    showTimer = new QTimer(this);
    showTimer->setInterval(300);
    connect(showTimer, &QTimer::timeout, this, &BaseWidget::handleShowTimerTimeout);
}

//设置是否需要调用显示后处理
void BaseWidget::setShowAfter(bool showAfter)
{
    bShowAfter = showAfter;
}


//处理显示定时器事件
void BaseWidget::handleShowTimerTimeout()
{
    showAfter();
    showTimer->stop();
    bFirstShow = false;
}


//重新实现事件处理函数
void BaseWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        languageChange();
    }
    else {
        QWidget::changeEvent(event);
    }
}

//处理窗口显示事件
void BaseWidget::showEvent(QShowEvent *event)
{
    if (event->type() == QEvent::Show) {
        // 初始化显示定时器
        if (showTimer == nullptr) {
            initShowTimer();
        }

        if(bFirstShow)//首次显示时调用showAfter()方法
        {
            showTimer->start();
        }
        else if(bShowAfter)//后续是否需要调用showAfter()方法，根据bShowAfter设置
        {
            showTimer->start();
        }
    }
    else {
        QWidget::showEvent(event);
    }
}
