#include <cassert>
#include <unordered_map>

#include "logger.h"
#include "interrupt_handler.h"
#include "os.h"
#include "exccommon.h"


static thread_local bool g_withinInterProtect = false;
static thread_local bool g_signalOccurred = false;
static thread_local std::vector<bool> g_occurred_sigs{};
// map of signal and index into the g_occurred_sigs vector
static thread_local std::unordered_map<int, int> g_sig_indeces{};


#ifdef __cplusplus
extern "C" {
#endif

void ip_dummySighandler(int signum){
    auto it = g_sig_indeces.find(signum);
    if(it == g_sig_indeces.end()){
        const char msg[] = "ip_dummySighandler: error: failed to find signal...\n";
        os::write(2, msg, sizeof(msg)-1);
        return;
    }
    g_occurred_sigs[it->second] = true;
    g_signalOccurred = true;
}

#ifdef __cplusplus
}
#endif


InterruptProtect::InterruptProtect()
{}

InterruptProtect::InterruptProtect(int signum) :
    InterruptProtect(std::vector<int>{signum})
{}

InterruptProtect::InterruptProtect(const std::vector<int> &sigs){
    this->enable(sigs);
}

bool InterruptProtect::signalOccurred()
{
    return g_signalOccurred;
}


InterruptProtect::~InterruptProtect()
{
    if(! g_withinInterProtect){
        return;
    }
    try {
        this->disable();
    }  catch (const std::exception& e) {
        logCritical << __func__ << e.what();
    }
}

void InterruptProtect::enable(const std::vector<int> &sigs)
{
    if(g_withinInterProtect){
        throw QExcProgramming(QString(__func__) + ": only one instance allowed per thread");
    }
    g_withinInterProtect = true;
    g_signalOccurred = false;
    m_sigs = sigs;
    m_oldActions.resize(sigs.size());

    g_occurred_sigs.resize(sigs.size(), false);

    struct sigaction act{};
    act.sa_handler = ip_dummySighandler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;
    for(int idx=0; idx < int(sigs.size()); idx++){
        auto s = sigs[idx];
        g_sig_indeces[s] = idx;
        os::sigaction(s, &act, &m_oldActions[idx]);
    }
}

void InterruptProtect::disable()
{
    if(! g_withinInterProtect){
        throw QExcProgramming(QString(__func__) + ": not enabled");
    }
    // restore previous handlers
    for(int idx=0; idx < int(m_sigs.size()); idx++){
        try {
            os::sigaction(m_sigs[idx], &m_oldActions[idx], nullptr);
        }  catch (const os::ExcOs& e) {
            logCritical << e.what();
        }
    }
    g_withinInterProtect = false;
    if(! g_signalOccurred){
        return;
    }
    for (auto& it: g_sig_indeces) {
        auto sig = it.first;
        auto idx = it.second;
        if(g_occurred_sigs.at(idx)){
            logDebug << "sending sig" << sig;
            kill(getpid(), sig);
        }
    }
}
