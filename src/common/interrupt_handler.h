#pragma once

#include <csignal>
#include <QVarLengthArray>

/// Defer processing of the signal SIGINT
/// until destruction. Automatically restart (some)
/// system-calls during that time (SA_RESTART).
/// Only one instance allowed at a time per thread!
class InterruptProtect
{
public:    
    InterruptProtect(int signum=SIGINT);
    bool signalOccurred();

    ~InterruptProtect();

public:
    Q_DISABLE_COPY(InterruptProtect)
private:
    struct sigaction m_oldAct{};
    int m_signum;
};

