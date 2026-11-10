#include "mainwidget.h"

#include <QApplication>
#include "LogHandler.h"
#include "BaseConfigSetting.h"
#include "Language.h"
#include "BaseStytle.h"

int main(int argc, char *argv[])
{
    // 安装日志处理器，将 qDebug 输出重定向到文件
    LogHandler::install();

    QApplication a(argc, argv);
    
    // 测试日志输出
    qDebug() << "Application started";
    qInfo() << "Qt version:" << QT_VERSION_STR;
    
    // 加载样式
    BaseStytle::getInstance()->loadStyle();

    // 加载配置
    BaseConfigSetting::getInstance();
    //加载语言类
    Language::getInstance();

    
    MainWidget w;
    w.show();    
    return a.exec();
}
