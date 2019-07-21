#include "shell_globals.h"

ShellGlobals &ShellGlobals::instance()
{
    static ShellGlobals s;
    return s;
}

ShellGlobals::ShellGlobals() :
    shournalSocketNb(-1),
    watchState(E_WatchState::DISABLED),
    inSubshell(false),
    lastMountNamespacePid(-1),
    pAttchedShell(nullptr),
    verbosityLevel(QtMsgType::QtWarningMsg),
    shournalSockFdDescripFlags(-1),
    shournalRootDirFd(-1)
{
     ignoreEvents.clear();
     ignoreSigation.clear();
}
