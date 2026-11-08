#ifndef BASEWIDGET_H
#define BASEWIDGET_H
#include <QWidget>
#include <QTimer>
#include <QShowEvent>

class BaseWidget : public QWidget
{
    Q_OBJECT
public:
    BaseWidget(QWidget *parent = nullptr);
    ~BaseWidget();

    //语言切换时调用
    virtual void languageChange() = 0;

    //窗口显示后调用
    virtual void showAfter(){}

    //初始化显示定时器
    void initShowTimer();


public://对外暴露的方法
    bool isFirstShow() const { return bFirstShow; }
    void setShowAfter(bool showAfter);

   public slots:
    //处理显示定时器事件
    void handleShowTimerTimeout();
protected:
    //重新实现事件处理函数
    void changeEvent(QEvent *event) override;

    //处理窗口显示事件
    void showEvent(QShowEvent *event) override;

private:
    //显示定时器,延迟300ms调用showAfter()方法，避免未初始化完成时调用showAfter()方法
    QTimer *showTimer;

    //是否需要调用显示后处理，默认只需首次显示时调用
    bool bShowAfter;

    //是否首次显示
    bool bFirstShow;

};



#endif // BASEWIDGET_H
