#include "qoptargtrigger.h"

#include <utility>

QOptArgTrigger::QOptArgTrigger() = default;

QOptArgTrigger::QOptArgTrigger(QOptArgTrigger::TriggerEntries trigger) :
    m_trigger(std::move(trigger))
{

}

const QOptArgTrigger::TriggerEntries &QOptArgTrigger::trigger() const
{
    return m_trigger;
}

bool QOptArgTrigger::isEmpty() const
{
    return m_trigger.isEmpty();
}

void QOptArgTrigger::setTrigger(const TriggerEntries &trigger)
{
    m_trigger = trigger;
}


