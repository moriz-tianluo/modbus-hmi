#ifndef PROJECTCONFIG_H
#define PROJECTCONFIG_H

#include <QObject>

// 项目级配置持久化占位类。
// 预期职责（后续实现时按需要补充）：
//   - 串口参数最近一次使用值的记忆（下次打开自动回填）
//   - 轮询周期/最大重试次数等运行期偏好保存（QSettings 写注册表或 INI）
//   - 与 ChannelModel 的 JSON 配置协作：project config 存"上次选择的配置文件路径"
//   - 不涉及通道本身的业务数据（那是 ChannelModel 的职责）
//
// 当前仅为空壳，等待后续需求明确后填充成员变量与加载/保存方法。
class ProjectConfig : public QObject
{
    Q_OBJECT
public:
    explicit ProjectConfig(QObject *parent = nullptr);

signals:
};

#endif // PROJECTCONFIG_H
