#include "stupidinject.h"


void StupidInject::addInjection(const StupidInject::Action& action)
{
    m_actions.push_back(action);
}

void StupidInject::addInjection(const char *trigger, const char *replacement)
{
    Action act;
    act.trigger = trigger;
    act.func = [replacement](QTextStream &out){
        out << replacement;
    };
    m_actions.push_back(act);
}

void StupidInject::addInjection(const char *trigger, const std::function<void (QTextStream &)>& func)
{
    Action act;
    act.trigger = trigger;
    act.func = func;
    m_actions.push_back(act);
}


void StupidInject::stream(const char *input, QTextStream &out)
{
    const char* lastBegin = input;
    for(const auto& action : m_actions){
        const char *triggerInInput = strstr(lastBegin, action.trigger.constData());
        if(triggerInInput == nullptr){
            throw EqcInjectTriggerNotFound("Trigger not found: " + action.trigger);
        }
        // length of the string that has yet to be written before injecting
        auto lastInputLenght = triggerInInput - lastBegin;
        const auto lastInput = QByteArray::fromRawData(lastBegin, int(lastInputLenght));
        out << lastInput;
        action.func(out);
        lastBegin = triggerInInput + action.trigger.size();
    }
    // the rest still needs to be written
    out << lastBegin;
}
