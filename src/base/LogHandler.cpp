#include "LogHandler.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDebug>
#include <QDir>

// 日志文件最大大小：10MB
const qint64 MAX_LOG_SIZE = 10 * 1024 * 1024;

// 静态成员变量初始化
QMutex LogHandler::s_mutex;
QFile LogHandler::s_logFile;
QString LogHandler::s_logDir;

/**
 * @brief 设置日志文件存放目录
 * @param dirPath 日志目录路径
 * @return 是否设置成功
 */
bool LogHandler::setLogDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create log directory:" << dirPath;
            return false;
        }
    }
    s_logDir = dirPath;
    return true;
}

/**
 * @brief 安装日志处理器
 * @param logFilePath 日志文件完整路径（可选）
 */
void LogHandler::install(const QString& logFilePath) {
    QString path = logFilePath;
    if (path.isEmpty()) {
        // 使用默认目录或用户设置的目录
        QString dir = s_logDir.isEmpty() ? ".log" : s_logDir;
        
        // 确保目录存在
        QDir logDir(dir);
        if (!logDir.exists()) {
            logDir.mkpath(".");
        }
        
        // 默认日志文件名
        path = dir + "/des.log";
    }
    
    s_logFile.setFileName(path);
    if (s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
        qDebug() << "Log handler installed. Log file:" << path;
    } else {
        qWarning() << "Failed to open log file:" << path;
    }
}

/**
 * @brief 日志文件轮转
 * 
 * 将当前日志文件重命名归档，创建新文件继续记录
 */
void LogHandler::rotateLogFile() {
    QString currentPath = s_logFile.fileName();
    s_logFile.close();
    
    // 解析当前文件路径信息
    QFileInfo fileInfo(currentPath);
    QString baseName = fileInfo.baseName();
    QString suffix = fileInfo.suffix();
    QString dirPath = fileInfo.dir().path();
    
    // 生成归档文件名：des_yyyyMMdd_hhmmss.log
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString newName = QString("%1/%2_%3.%4").arg(dirPath, baseName, timestamp, suffix);
    
    // 重命名文件
    if (QFile::rename(currentPath, newName)) {
        qInfo() << "Log file rotated:" << newName;
    } else {
        qWarning() << "Failed to rotate log file:" << currentPath;
    }
    
    // 重新打开新的日志文件
    s_logFile.setFileName(currentPath);
    s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

/**
 * @brief Qt 消息处理器回调函数
 * @param type 消息类型
 * @param context 消息上下文
 * @param msg 消息内容
 */
void LogHandler::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QMutexLocker locker(&s_mutex);
    
    // 检查日志文件大小，达到阈值则轮转
    if (s_logFile.isOpen() && s_logFile.size() >= MAX_LOG_SIZE) {
        rotateLogFile();
    }
    
    // 根据消息类型确定日志级别
    QString logLevel;
    switch (type) {
    case QtDebugMsg:
        logLevel = "DEBUG";
        break;
    case QtInfoMsg:
        logLevel = "INFO";
        break;
    case QtWarningMsg:
        logLevel = "WARN";
        break;
    case QtCriticalMsg:
        logLevel = "CRITICAL";
        break;
    case QtFatalMsg:
        logLevel = "FATAL";
        break;
    default:
        logLevel = "UNKNOWN";
    }
    
    // 格式化日志消息：[时间戳] [级别] 消息
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] %3\n").arg(timestamp, logLevel, msg);
    
    // 写入日志文件
    QTextStream stream(&s_logFile);
    stream << logMessage;
    stream.flush();
    
    // 同时输出到控制台
    fprintf(stderr, "%s", logMessage.toUtf8().constData());
}