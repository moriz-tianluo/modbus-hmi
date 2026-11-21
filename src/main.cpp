#include "mainwidget.h"

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
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

    // 单实例保护：避免两个进程同时写 hmi.db / project.ini
    QLockFile lockFile(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                           .filePath(QStringLiteral("modbus-hmi.lock")));
    lockFile.setStaleLockTime(30000);
    if (!lockFile.tryLock(200)) {
        QMessageBox::information(nullptr, LANGUAGE("LG_app_alreadyRunningTitle"),
                                 LANGUAGE("LG_app_alreadyRunningText"));
        return 0;
    }

    MainWidget w;
    w.show();    
    return a.exec();
}
