#pragma once

#include <functional>

#include <QTextStream>
#include <QVector>

#include "exccommon.h"

class EqcInjectTriggerNotFound : public QExcCommon
{
public:
    using QExcCommon::QExcCommon;
};


/// "Inject" arbitrary content into a text-stream. The actions
/// have to be added in the order of their later occurence
/// within the input-stream.
class StupidInject
{
public:
    struct Action {
        std::function< void(QTextStream &out)> func;
        QByteArray trigger;

    };

    void addInjection(const Action& action);
    void addInjection(const char* trigger, const char* replacement);
    void addInjection(const char* trigger,const std::function< void(QTextStream &out)>& func);

    void stream(const char* input, QTextStream& out);

private:
    QVector<Action> m_actions;
};

