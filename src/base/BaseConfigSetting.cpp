#include "BaseConfigSetting.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QString>
#include <QApplication>
#include <QStandardPaths>


// 单例配置设置类
BaseConfigSetting *BaseConfigSetting::instance = nullptr;

BaseConfigSetting::BaseConfigSetting(QObject *parent)
    : QObject(parent)
{
    init();
}

BaseConfigSetting::~BaseConfigSetting()
{
    delete configSettings;
}

BaseConfigSetting *BaseConfigSetting::getInstance()
{
    if (instance == nullptr)
    {
        instance = new BaseConfigSetting();
    }
    return instance;
}
// 初始化配置设置
void BaseConfigSetting::init()
{
    initConfig();
}
// 初始化配置参数
void BaseConfigSetting::initConfig()
{
    QString localAppDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (localAppDataPath.isEmpty())
    {
        qWarning() << "no local app data path found";
    }
    // 确保目录存在
    QDir localAppDataDir(localAppDataPath);
    if (!localAppDataDir.exists())
    {
        if (!localAppDataDir.mkpath("."))
        {
            qWarning() << "failed to create local app data dir:" << localAppDataPath;
            return;
        }
    }

    // 构建完整文件路径
    QString filePath = localAppDataDir.filePath("config.ini");

    // 检查文件是否存在，如果不存在则创建
    QFile file(filePath);
    if (!file.exists())
    {
        if (!file.open(QIODevice::WriteOnly))
        {
            qWarning() << "no config file found, create it:" << filePath << "error:" << file.errorString();
            return;
        }
        // 关闭文件
        file.close();
        qDebug() << "config file created successfully:" << filePath;
    }
    else
    {
        qDebug() << "config file already exists:" << filePath;
    }
    configSettings = new QSettings(filePath, QSettings::IniFormat);
}
// 保存配置参数到文件
void BaseConfigSetting::saveConfig()
{
    configSettings->sync();
    qDebug() << "Config file saved successfully.";
}
// 加载配置参数从文件
void BaseConfigSetting::loadConfig()
{
    initConfig();
}
