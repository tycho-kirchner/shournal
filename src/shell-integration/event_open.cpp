 #define _LARGEFILE64_SOURCE

#include <climits>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "event_open.h"
#include "logger.h"

#include "shell_globals.h"
#include "cleanupresource.h"
#include "qoutstream.h"
#include "translation.h"
#include "shell_request_handler.h"


int event_open::handleOpen(const char *pathname, int flags, mode_t mode, bool largeFile)
{
    if(largeFile){
        setBitIn(flags, O_LARGEFILE);
    }

    auto& g_shell = ShellGlobals::instance();
    if(pathname[0] == '\0' ||
            g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_open(pathname, flags, mode);
    }
    auto clearIgnEvents = finally([&g_shell] { g_shell.ignoreEvents.clear(); });

    if(! g_shell.inSubshell){
        if(shell_request_handler::checkForTriggerAndHandle()){
            return g_shell.orig_open(pathname, flags, mode);
        }
    }

    if(g_shell.watchState != E_WatchState::WITHIN_CMD){
        return g_shell.orig_open(pathname, flags, mode);
    }

    char buf[PATH_MAX + 1];
    const char* actualPath;
    if(pathname[0] == '/'){
        actualPath = pathname;
    } else {
        if(realpath(pathname, buf) == nullptr){            
            logInfo << qtr("Failed to resolve relative path %1 (%2). "
                           "File events will not be registered.")
                       .arg(pathname, translation::strerror_l());
            return g_shell.orig_open(pathname, flags, mode);
        }
        actualPath = buf;
    }
    return openat(g_shell.shournalRootDirFd, actualPath + 1, flags, mode);
}

