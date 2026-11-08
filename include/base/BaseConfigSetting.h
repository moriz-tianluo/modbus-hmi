#ifndef BASECONFIGSETTING_H
#define BASECONFIGSETTING_H

#include <QObject>
#include <QSettings>
#include <QColor>
#include <BaseStructData.h>



// 单例配置设置类
class BaseConfigSetting : public QObject
{
    Q_OBJECT
public:
    static BaseConfigSetting *getInstance();
    ~BaseConfigSetting();

public:
    // 初始化配置设置
    void init();
    void initConfig();

    // 保存配置参数到文件
    void saveConfig();
    // 加载配置参数从文件
    void loadConfig();

    
signals:
    // 配置参数改变信号(当配置参数改变时触发)
    void configChanged(const QString &key, const QVariant &value);

private slots:

private:
    explicit BaseConfigSetting(QObject *parent = nullptr);
    static BaseConfigSetting *instance;
private:
    QSettings *configSettings;
    QString configPath;
};

#endif // BASECONFIGSETTING_H
