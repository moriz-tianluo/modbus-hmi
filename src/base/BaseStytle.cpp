#include "BaseStytle.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

#include <algorithm>

BaseStytle *BaseStytle::instance = nullptr;

BaseStytle::BaseStytle(QObject *parent)
    : QObject(parent)
{
    initDefaultStylePaths();
}

BaseStytle::~BaseStytle()
{
}

BaseStytle *BaseStytle::getInstance()
{
    if (!instance) {
        instance = new BaseStytle();
    }
    return instance;
}

void BaseStytle::initDefaultStylePaths()
{
    // 默认样式资源列表，新增样式文件在此添加
    m_stylePathList = {
        ":/stytle/appstytle.css",
        ":/stytle/basestytle.css"
    };
    // 默认颜色配置文件
    m_colorConfigPath = ":/stytle/color.ini";
}

void BaseStytle::loadStyle()
{
    // 先加载颜色配置，用于替换样式文件中的颜色变量
    loadColorConfig(m_colorConfigPath);

    QString mergedStyle;

    for (const QString &path : m_stylePathList) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open style resource file:" << path << file.errorString();
            continue;
        }
        mergedStyle += QLatin1String(file.readAll());
        mergedStyle += "\n";
        file.close();
        qDebug() << "Style loaded:" << path;
    }

    if (mergedStyle.isEmpty()) {
        qWarning() << "No style content loaded from resource list.";
        return;
    }

    // 替换样式内容中的颜色变量
    m_currentStyle = replaceColorVariables(mergedStyle);
    warnUnresolvedVariables(m_currentStyle, m_stylePathList.join(", "));

    if (qApp) {
        qApp->setStyleSheet(m_currentStyle);
        qDebug() << "All styles applied successfully.";
    }

    emit styleChanged(m_currentStyle);
}

void BaseStytle::loadStyleFromFile(const QString &filePath)
{
    // 先加载颜色配置，用于替换样式文件中的颜色变量
    loadColorConfig(m_colorConfigPath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open style file:" << filePath << file.errorString();
        return;
    }

    QString rawStyle = QLatin1String(file.readAll());
    file.close();

    // 替换样式内容中的颜色变量
    m_currentStyle = replaceColorVariables(rawStyle);
    warnUnresolvedVariables(m_currentStyle, filePath);

    if (qApp) {
        qApp->setStyleSheet(m_currentStyle);
        qDebug() << "Style loaded from file:" << filePath;
    }

    emit styleChanged(m_currentStyle);
}

void BaseStytle::loadStyleFromString(const QString &styleStr)
{
    m_currentStyle = styleStr;

    if (qApp) {
        qApp->setStyleSheet(m_currentStyle);
        qDebug() << "Style loaded from string.";
    }

    emit styleChanged(m_currentStyle);
}

void BaseStytle::addStylePath(const QString &path)
{
    if (!m_stylePathList.contains(path)) {
        m_stylePathList.append(path);
        qDebug() << "Style path added:" << path;
    }
}

void BaseStytle::setColorConfigPath(const QString &path)
{
    if (m_colorConfigPath != path) {
        m_colorConfigPath = path;
        qDebug() << "Color config path set:" << path;
    }
}

QString BaseStytle::colorConfigPath() const
{
    return m_colorConfigPath;
}

void BaseStytle::loadColorConfig(const QString &filePath)
{
    if (filePath.isEmpty()) {
        qWarning() << "Color config path is empty, skip loading.";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open color config file:" << filePath << file.errorString();
        return;
    }

    m_colorVariables.clear();
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // 跳过空行与注释行（以 # 或 ; 开头）
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }
        int eqPos = line.indexOf('=');
        if (eqPos <= 0) {
            continue;
        }
        QString key = line.left(eqPos).trimmed();
        QString value = line.mid(eqPos + 1).trimmed();
        if (!key.isEmpty() && !value.isEmpty()) {
            m_colorVariables.insert(key, value);
        }
    }
    file.close();
    qDebug() << "Color config loaded from:" << filePath
             << "variables count:" << m_colorVariables.size();
}

QString BaseStytle::replaceColorVariables(const QString &styleContent) const
{
    if (m_colorVariables.isEmpty()) {
        return styleContent;
    }

    // 按变量名长度降序替换，避免短变量名先替换后破坏长变量名
    QStringList keys = m_colorVariables.keys();
    std::sort(keys.begin(), keys.end(), &BaseStytle::compareByNameLengthDesc);

    QString result = styleContent;
    for (const QString &key : keys) {
        result.replace(key, m_colorVariables.value(key));
    }
    return result;
}

QStringList BaseStytle::unresolvedVariableNames(const QString &styleContent) const
{
    QStringList names;
    static const QRegularExpression pattern(QStringLiteral("\\$[A-Za-z_][A-Za-z0-9_]*"));

    QRegularExpressionMatchIterator it = pattern.globalMatch(styleContent);
    while (it.hasNext()) {
        const QString name = it.next().captured(0);
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

void BaseStytle::warnUnresolvedVariables(const QString &styleContent, const QString &source) const
{
    const QStringList names = unresolvedVariableNames(styleContent);
    if (names.isEmpty()) {
        return;
    }

    qWarning() << "Unresolved style variables in" << source
               << ":" << names.join(", ")
               << "| color config:" << m_colorConfigPath;
}

bool BaseStytle::compareByNameLengthDesc(const QString &left, const QString &right)
{
    if (left.size() != right.size()) {
        return left.size() > right.size();
    }
    return left > right;
}

QString BaseStytle::currentStyle() const
{
    return m_currentStyle;
}

QStringList BaseStytle::stylePathList() const
{
    return m_stylePathList;
}
