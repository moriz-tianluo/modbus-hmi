#ifndef LOGHANDLER_H
#define LOGHANDLER_H

#include <QString>
#include <QMutex>
#include <QFile>

/**
 * @class LogHandler
 * @brief 日志处理器类，用于将 Qt 消息输出重定向到文件
 * 
 * 该类提供了将 qDebug、qInfo、qWarning、qCritical、qFatal 等
 * Qt 消息输出重定向到日志文件的功能，并支持日志文件自动分割。
 */
class LogHandler {
public:
    /**
     * @brief 设置日志文件存放目录
     * @param dirPath 日志目录路径（如 ".log" 或 "/var/log/myapp"）
     * @return 是否设置成功
     * 
     * 该方法必须在 install() 之前调用才会生效。
     * 如果目录不存在，会自动创建。
     */
    static bool setLogDirectory(const QString& dirPath);

    /**
     * @brief 安装日志处理器
     * @param logFilePath 日志文件完整路径（可选）
     * 
     * 如果不提供 logFilePath 参数，会使用默认路径：
     * [日志目录]/des.log（日志目录默认为 ".log"）
     * 
     * 安装后，所有 Qt 消息输出（qDebug、qInfo 等）都会被重定向到日志文件。
     */
    static void install(const QString& logFilePath = QString());

private:
    /**
     * @brief Qt 消息处理器回调函数
     * @param type 消息类型（QtDebugMsg、QtInfoMsg、QtWarningMsg 等）
     * @param context 消息上下文（包含文件名、行号等信息）
     * @param msg 消息内容
     * 
     * 该函数会被 Qt 框架自动调用，处理所有消息输出。
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    /**
     * @brief 日志文件轮转
     * 
     * 当日志文件大小达到阈值（10MB）时，将当前日志文件重命名归档，
     * 归档文件名格式：des_yyyyMMdd_hhmmss.log，然后创建新的日志文件继续记录。
     */
    static void rotateLogFile();

    static QMutex s_mutex;       ///< 互斥锁，保证多线程安全
    static QFile s_logFile;      ///< 日志文件对象
    static QString s_logDir;     ///< 日志目录路径
};

#endif // LOGHANDLER_H