#include "Language.h"
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QDomNode>

#include <QDebug>


Language *Language::instance = nullptr;

Language::Language()
{
    languageType = LanguageType_Zh;
    init();
}

Language::~Language()
{
}

Language *Language::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Language();
    }
    return instance;
}

LanguageType Language::convertToLanguageType(QLocale::Language lang)
{
    switch (lang)
    {
    case QLocale::English:
        return LanguageType_En;
    case QLocale::Chinese:
        return LanguageType_Zh;
    default:
        return LanguageType_Zh;
    }
}



QString Language::getLanguageString(const QString &key)
{
    if (languageMap.contains(languageType))
    {
        if (languageMap[languageType].contains(key))
        {
            return languageMap[languageType][key];
        }
    }
    return key;
}



void Language::setLanguageType(LanguageType type)
{
    languageType = type;
}

LanguageType Language::getLanguageType() const
{
    return languageType;
}

void Language::init()
{
    readChineseXML("language/chinese.xml");
    readEnglishXML("language/english.xml");
}

void Language::readChineseXML(const QString &xmlPath)
{
    readLanguageStringsFromXML(LanguageType_Zh, xmlPath);
}

void Language::readEnglishXML(const QString &xmlPath)
{
    readLanguageStringsFromXML(LanguageType_En, xmlPath);
}

bool Language::readLanguageStringsFromXML(LanguageType type, const QString &xmlPath)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "open file failed:" << xmlPath;
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(&file))
    {
        file.close();
        qDebug() << "parse xml failed:" << xmlPath;
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.isNull())
        return false;

    // 临时存放当前语言的键值对
    QMap<QString, QString> langKv;

    // 遍历所有 <data> 节点
    QDomNodeList dataList = root.elementsByTagName("data");
    for (int i = 0; i < dataList.size(); ++i)
    {
        QDomElement ele = dataList.at(i).toElement();
        QString key   = ele.attribute("key");
        QString value = ele.attribute("value");
        if (!key.isEmpty())
        {
            langKv.insert(key, value);
        }
    }

    // 存入外层语言映射
    languageMap.insert(type, langKv);
    return true;
}
