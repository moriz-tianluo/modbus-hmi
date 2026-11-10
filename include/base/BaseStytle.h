#ifndef BASESTYTLE_H
#define BASESTYTLE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

// 单例样式管理类
class BaseStytle : public QObject
{
    Q_OBJECT
public:
    static BaseStytle *getInstance();
    ~BaseStytle();

    // 从资源列表加载所有样式
    void loadStyle();

    // 从指定路径加载单个样式文件
    void loadStyleFromFile(const QString &filePath);

    // 加载自定义样式字符串
    void loadStyleFromString(const QString &styleStr);

    // 添加样式文件路径到加载列表（动态新增）
    void addStylePath(const QString &path);

    // 设置颜色配置文件路径（color.ini）
    void setColorConfigPath(const QString &path);

    // 获取颜色配置文件路径
    QString colorConfigPath() const;

    // 获取当前合并后的样式内容
    QString currentStyle() const;

    // 获取样式文件列表
    QStringList stylePathList() const;

signals:
    // 样式变化信号
    void styleChanged(const QString &style);

private:
    explicit BaseStytle(QObject *parent = nullptr);
    static BaseStytle *instance;

    // 初始化默认样式文件列表
    void initDefaultStylePaths();

    // 从指定路径加载颜色配置文件（color.ini）
    void loadColorConfig(const QString &filePath);

    // 使用颜色变量替换样式内容中的占位符（如 $base_color）
    QString replaceColorVariables(const QString &styleContent) const;

    // 取出样式内容中尚未被替换的变量名列表
    QStringList unresolvedVariableNames(const QString &styleContent) const;

    // 存在未替换变量时输出警告日志
    void warnUnresolvedVariables(const QString &styleContent, const QString &source) const;

    // 变量名长度降序比较，避免前缀变量覆盖长变量名（如 $warningRed 覆盖 $warningRedDark）
    static bool compareByNameLengthDesc(const QString &left, const QString &right);

private:
    QString m_currentStyle;
    QStringList m_stylePathList;
    QString m_colorConfigPath;
    QMap<QString, QString> m_colorVariables;
};

#endif // BASESTYTLE_H
