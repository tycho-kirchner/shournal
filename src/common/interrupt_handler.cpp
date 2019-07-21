#include <cassert>

#include "logger.h"
#include "interrupt_handler.h"
#include "os.h"
#include "exccommon.h"


static thread_local bool g_signalOccurred = false;
static thread_local bool g_withinInterProtect = false;


#ifdef __cplusplus
extern "C" {
#endif

void dummySighandler(int){
    g_signalOccurred = true;
}

#ifdef __cplusplus
}
#endif


InterruptProtect::InterruptProtect()
{
    if(g_withinInterProtect){
        throw QExcProgramming(QString(__func__) + ": only one instance allowed per thread");
    }
    g_withinInterProtect = true;
    g_signalOccurred = false;

    struct sigaction act{};
    act.sa_handler = dummySighandler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;

    os::sigaction(SIGINT, &act, &m_oldAct);
}


InterruptProtect::~InterruptProtect()
{    
    // restore previous handler
    try {
        os::sigaction(SIGINT, &m_oldAct, nullptr);
    } catch (const os::ExcOs& e) {
        logCritical << e.what();
    }
    g_withinInterProtect = false;

    if(g_signalOccurred){
        kill(getpid(), SIGINT);
    }


}
