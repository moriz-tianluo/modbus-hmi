#ifndef SIGNALS_H
#define SIGNALS_H

#include <QObject>
#include "BaseStructData.h"

class Signals : public QObject
{
    Q_OBJECT
public:
    static Signals& getInstance()
    {
        static Signals instance;
        return instance;
    }

signals:
    void updateSignal();
private:
    Signals(QObject *parent = nullptr) : QObject(parent) {}
    ~Signals() {}
    Signals(const Signals&) = delete;
    Signals& operator=(const Signals&) = delete;
};

#define SignalsInstance (&Signals::getInstance())

#endif // SIGNALS_H
