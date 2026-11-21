#include <QtTest>

#include <QDomDocument>
#include <QFile>
#include <QSet>
#include <QStringList>

#ifndef HMI_LANGUAGE_DIR
#define HMI_LANGUAGE_DIR ""
#endif

// 语言包一致性测试：中英文键集合必须完全一致，且同一文件内不允许重复键。
class TestLanguageKeys : public QObject
{
    Q_OBJECT

private slots:
    void chineseAndEnglishHaveSameKeys();
    void noDuplicateKeys();

private:
    QString chinesePath() const;
    QString englishPath() const;
    QStringList keysOf(const QString &xmlPath) const;
};

QString TestLanguageKeys::chinesePath() const
{
    return QStringLiteral(HMI_LANGUAGE_DIR "/chinese.xml");
}

QString TestLanguageKeys::englishPath() const
{
    return QStringLiteral(HMI_LANGUAGE_DIR "/english.xml");
}

QStringList TestLanguageKeys::keysOf(const QString &xmlPath) const
{
    QStringList keys;

    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return keys;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return keys;
    }
    file.close();

    const QDomNodeList dataList = doc.documentElement().elementsByTagName(QStringLiteral("data"));
    for (int i = 0; i < dataList.size(); ++i) {
        const QString key = dataList.at(i).toElement().attribute(QStringLiteral("key"));
        if (!key.isEmpty()) {
            keys.append(key);
        }
    }
    return keys;
}

void TestLanguageKeys::chineseAndEnglishHaveSameKeys()
{
    const QStringList chineseKeys = keysOf(chinesePath());
    const QStringList englishKeys = keysOf(englishPath());

    QVERIFY2(!chineseKeys.isEmpty(), "无法读取中文语言包，请检查 HMI_LANGUAGE_DIR");
    QVERIFY2(!englishKeys.isEmpty(), "无法读取英文语言包，请检查 HMI_LANGUAGE_DIR");

    const QSet<QString> chineseSet(chineseKeys.begin(), chineseKeys.end());
    const QSet<QString> englishSet(englishKeys.begin(), englishKeys.end());

    const QSet<QString> missingInEnglish = chineseSet - englishSet;
    const QSet<QString> missingInChinese = englishSet - chineseSet;

    QStringList missingInEnglishList(missingInEnglish.begin(), missingInEnglish.end());
    QStringList missingInChineseList(missingInChinese.begin(), missingInChinese.end());
    missingInEnglishList.sort();
    missingInChineseList.sort();

    QVERIFY2(missingInEnglish.isEmpty(),
             qPrintable(QStringLiteral("英文语言包缺少键：%1")
                            .arg(missingInEnglishList.join(QStringLiteral(", ")))));
    QVERIFY2(missingInChinese.isEmpty(),
             qPrintable(QStringLiteral("中文语言包缺少键：%1")
                            .arg(missingInChineseList.join(QStringLiteral(", ")))));
}

void TestLanguageKeys::noDuplicateKeys()
{
    const QStringList chineseKeys = keysOf(chinesePath());
    const QStringList englishKeys = keysOf(englishPath());

    const QSet<QString> uniqueChinese(chineseKeys.begin(), chineseKeys.end());
    const QSet<QString> uniqueEnglish(englishKeys.begin(), englishKeys.end());

    QCOMPARE(uniqueChinese.size(), chineseKeys.size());
    QCOMPARE(uniqueEnglish.size(), englishKeys.size());
}

QTEST_GUILESS_MAIN(TestLanguageKeys)

#include "test_languagekeys.moc"
