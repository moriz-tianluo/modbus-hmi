#ifndef POLLTASK_H
#define POLLTASK_H

#include "modbusrtu.h"

#include <QMetaType>
#include <QVector>

// 单个轮询任务：一个从站上的一段连续寄存器/线圈读取。
// 多从站、多段地址时，由 ChannelModel::buildPollTasks() 按通道配置自动分段生成。
struct PollTask {
    quint8 slaveId = 1;                                        // 从站地址
    ModbusFunctionCode functionCode = ModbusFunctionCode::ReadHoldingRegisters;
    quint16 startAddress = 0;                                  // 起始寄存器地址
    quint16 quantity = 0;                                      // 读取数量
};

Q_DECLARE_METATYPE(PollTask)
Q_DECLARE_METATYPE(QVector<PollTask>)

#endif // POLLTASK_H
