#pragma once

#include <csignal>
#include <qglobal.h>
#include <vector>

#include "util.h"

/// Defer processing of signals
/// until destruction. Automatically restart (some)
/// system-calls during that time (SA_RESTART).
/// Only one instance allowed at a time per thread!
class InterruptProtect
{
public:    
    InterruptProtect();
    InterruptProtect(int signum);
    InterruptProtect(const std::vector<int> &sigs);
    ~InterruptProtect();

    void enable(const std::vector<int> &sigs);
    void disable();
    bool signalOccurred();


public:
    Q_DISABLE_COPY(InterruptProtect)
    DEFAULT_MOVE(InterruptProtect)
private:
    std::vector<int> m_sigs{};
    std::vector<struct sigaction> m_oldActions{};
};

