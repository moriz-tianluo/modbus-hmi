#ifndef LANGUAGE_H
#define LANGUAGE_H

//根据键值获取语言字符串
//通过单例类获取语言字符串
#include <QString>
#include <QMap>
#include <QLocale>

//语言类型
enum LanguageType
{
    LanguageType_En = 0,
    LanguageType_Zh = 1,
};



class Language
{
public:
    ~Language();
    //获取单例实例
    static Language *getInstance();
    //根据QT的语言枚举转换为语言类型
    LanguageType convertToLanguageType(QLocale::Language lang);
    
       //获取语言字符串    
    QString getLanguageString(const QString &key);

    //设置语言类型
    void setLanguageType(LanguageType type);
    
    //获取语言类型
    LanguageType getLanguageType() const;

    //初始化
    void init();

    //读取中文XML文件
    void readChineseXML(const QString &xmlPath);
    //读取英文XML文件
    void readEnglishXML(const QString &xmlPath);
    //读取XML中的语言字符串
    bool readLanguageStringsFromXML(LanguageType type, const QString &xmlPath);

private:
    Language();
    LanguageType languageType;
    static Language *instance;

    //语言字符串映射表
    QMap<LanguageType, QMap<QString, QString>> languageMap;
};

#define LANGUAGE_INSTANCE Language::getInstance()
#define LANGUAGE(key) LANGUAGE_INSTANCE->getLanguageString(key)



#endif // LANGUAGE_H
