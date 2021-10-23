 #define _LARGEFILE64_SOURCE

#include <climits>
#include <cstdlib>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "event_open.h"
#include "logger.h"

#include "shell_globals.h"
#include "cleanupresource.h"
#include "qoutstream.h"
#include "osutil.h"
#include "shell_request_handler.h"
#include "shell_logger.h"
#include "translation.h"

using shell_request_handler::ShellRequest;
using shell_request_handler::checkForTriggerAndHandle;

/// @return absolute version of the passed path or an empty string in case
/// of an error.
static std::string mkAbsPath(const char* path){
    if(path[0] == '/'){
        return path;
    }
    std::string buf(PATH_MAX, '\0');

    char* rawBuf = strDataAccess(buf);
    if(getcwd(rawBuf, buf.size()) == nullptr){
        logWarning << qtr("Failed to resolve relative path %1. "
                       "The working-directory could not be determined (%2). "
                       "File events will not be registered.")
                   .arg(path, translation::strerror_l());
        return {};
    }
    if(rawBuf[0] != '/'){
        // see also man 3 getcwd
        logWarning << qtr("Failed to resolve relative path %1. "
                       "The working-directory does not begin with '/' but %2. "
                       "File events will not be registered.")
                   .arg(path, rawBuf);
        return {};
    }

    // resize to actual length
    buf.resize(strlen(rawBuf));
    if(buf.size() != 1){
        buf += '/';
    }
    buf += path;
    return buf;
}

/// Write to a new unnamed tmp-file, if the shell-request
/// was successful. Note that the shell will close the fd
/// for us later.
/// @return fd to deleted tmp-file.
static int writerTriggerResponse(bool success){
    int fd =  osutil::unnamed_tmp();
    std::string mesg = (success) ? "ok" : "fail";
    // write string null-terminated (size+1) so in the shell
    // we can read -d '' trigger_response < '_///shournal_trigger_response///_'
    os::write(fd, mesg.c_str(), mesg.size() + 1);
    os::lseek(fd, 0, SEEK_SET);
    return fd;
}

int event_open::handleOpen(const char *pathname, int flags, mode_t mode, bool largeFile)
{
    if(largeFile){
        setBitIn(flags, O_LARGEFILE);
    }

    auto& g_shell = ShellGlobals::instance();
    if(g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_open(pathname, flags, mode);
    }
    auto clearIgnEvents = finally([&g_shell] { g_shell.ignoreEvents.clear(); });

    // Note: we only process shell-request if the trigger env-variable is set AND the current
    // pathname is _///shournal_trigger_response///_
    // So check for the pathname before calling handling the request in
    // checkForTriggerAndHandle (this is for cases where the trigger variable is set
    // and other redirections occurr in between).
    if(! g_shell.inSubshell &&
       strcmp(pathname, "_///shournal_trigger_response///_") == 0){
        bool shellRequestSuccess = false;
        auto shellRequest = checkForTriggerAndHandle(&shellRequestSuccess);
        switch (shellRequest) {
            case ShellRequest::TRIGGER_UNSET: break;
            default:
                return writerTriggerResponse(shellRequestSuccess);
            }
    }

    if(g_shell.watchState != E_WatchState::WITHIN_CMD){
        shell_earlydbg("ignoring pathname %s (not WITHIN_CMD)", pathname);
        return g_shell.orig_open(pathname, flags, mode);
    }
    const auto absPath = mkAbsPath(pathname);

    // pass the resolved abs. path relative to shournal's root directory fd,
    // by omitting the initial '/'.
    // Users may further pass malformed file-paths such as //foo, so find the first
    // non-slash char.
    const char* actualPath = nullptr;
    for(size_t i=0; i < absPath.size(); i++){
        if(absPath[i] != '/'){
            actualPath = &absPath[i];
            break;
        }
    }

    if(actualPath == nullptr || absPath.c_str() + absPath.size() - actualPath < 1){
        // Get here on mkAbsPath-error or because user attempted to open "/" or ""
        // The shortest possible absolute FILEpath under linux is two chars long.
        // We may get here, if bash-user calls e.g.
        // while read line; do echo $line ; done < "/"
        logDebug << "no valid path" << absPath;
        return g_shell.orig_open(pathname, flags, mode);
    }

    logDebug << "about to open" << actualPath - 1;
    return openat(g_shell.shournalRootDirFd, actualPath, flags, mode);
}

