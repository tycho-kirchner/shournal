#pragma once

#include <csignal>
#include <qglobal.h>
#include <vector>

/// Defer processing of signals
/// until destruction. Automatically restart (some)
/// system-calls during that time (SA_RESTART).
/// Only one instance allowed at a time per thread!
class InterruptProtect
{
public:    
    InterruptProtect(int signum);
    InterruptProtect(const std::vector<int> &sigs);
    bool signalOccurred();

    ~InterruptProtect();

public:
    Q_DISABLE_COPY(InterruptProtect)
private:
    std::vector<int> m_sigs{};
    std::vector<struct sigaction> m_oldActions{};
};

