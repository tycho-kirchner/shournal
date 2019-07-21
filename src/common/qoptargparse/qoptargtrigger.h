#pragma once

#include <unordered_set>
#include <QString>
#include <QHash>

#include "../util.h"

/// Allow the consumption of multiple commandline arguments, in case
/// a trigger word is given. Example:
/// If the trigger word <-between> is given, two values shall be consumed,
/// if <-greater> is given, one value shall be consumed.
class QOptArgTrigger
{
public:
    // store for each trigger, how many values shall be consumed
    typedef QHash<QString, int> TriggerEntries;

    QOptArgTrigger();
    QOptArgTrigger(TriggerEntries  pTrigger);


    const TriggerEntries& trigger() const;
    void setTrigger(const TriggerEntries &trigger);

    bool isEmpty() const;


private:
    TriggerEntries m_trigger;
};

