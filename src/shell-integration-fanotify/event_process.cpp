
#include <cassert>
#include <QStandardPaths>
#include <QCoreApplication>
#include <sys/socket.h>
#include <csignal>


#include "cleanupresource.h"
#include "logger.h"

#include "event_process.h"

#include "shell_globals.h"
#include "settings.h"
#include "excos.h"
#include "qsimplecfg/exccfg.h"
#include "subprocess.h"
#include "osutil.h"
#include "fdcommunication.h"
#include "util.h"
#include "commandinfo.h"
#include "app.h"
#include "shell_logger.h"
#include "translation.h"
#include "qoutstream.h"
#include "shell_request_handler.h"



static int execveUnobserved(const char *filename, char * const argv[], char * const envp[]){
    auto& g_shell = ShellGlobals::instance();
    logDebug << __func__ << filename;

    auto sockFlags = g_shell.shournalSockFdDescripFlags;
    setBitIn(sockFlags, FD_CLOEXEC);
    os::setFdDescriptorFlags(g_shell.shournalSocketNb, sockFlags);
    // in case execve fails, restore flags.
    auto resetCLOEXEC = finally([&g_shell] {
        try {
            os::setFdDescriptorFlags(g_shell.shournalSocketNb,
                                     g_shell.shournalSockFdDescripFlags);
        } catch (std::exception& e) {
            logCritical << e.what();
        }

    });
    return g_shell.orig_execve(filename, argv, envp);
}


pid_t event_process::handleFork()
{
    auto& g_shell = ShellGlobals::instance();
    if( g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_fork();
    }
    auto clearIgnEvents = finally([&g_shell] {g_shell.ignoreEvents.clear(); });

    switch (g_shell.watchState) {
    case E_WatchState::WITHIN_CMD: {
        pid_t ret = g_shell.orig_fork();
        if(ret == 0){
            g_shell.inSubshell = true;
        }
        return ret;
    }    
    default:
        break;
    }
    return g_shell.orig_fork();
}



int event_process::handleExecve(const char *filename, char * const argv[], char * const envp[])
{
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_execve(filename, argv, envp);
    }
    auto clearIgnEvents = finally([&g_shell] {g_shell.ignoreEvents.clear(); });

    if(! g_shell.inSubshell ||
         g_shell.watchState != E_WatchState::WITHIN_CMD){
        return g_shell.orig_execve(filename, argv, envp);
    }

    // No point in observing an unstattable executable.
    // Further, do not monitor suid-applications. Note: this is, of course, *not*
    // a security-feature, however, events by other users are in
    // relevant cases not recorded by shournal anyway.
    struct stat st;
    if(stat(filename, &st) == -1 || IsBitSet(st.st_mode, mode_t(S_ISUID) ) ){
        return execveUnobserved(filename, argv, envp);
    }

    std::string filenameStr(filename);
    auto& sets = Settings::instance();
    if(sets.ignoreCmdsRegardslessOfArgs().find(filenameStr) !=
            sets.ignoreCmdsRegardslessOfArgs().end()){
        return execveUnobserved(filename, argv, envp);
    }
    std::string fullCmd;

    QVarLengthArray<const char*, 8192> args;

    args.push_back(app::SHOURNAL_RUN_FANOTIFY);
    args.push_back("--msenter");
    std::string pid = std::to_string(g_shell.lastMountNamespacePid);
    args.push_back(pid.c_str());

    args.push_back("--verbosity");
    args.push_back(logger::msgTypeToStr(g_shell.verbosityLevel));

    args.push_back("--env");
    // first value after --env is its size, which we don't know yet.
    args.push_back("DUMMY");
    int envSizeIdx = args.size() -1;

    // set shournal socket only for observed processes (do not add to
    // shell env).
    const std::string shournalSocketNbStr = std::string(app::ENV_VAR_SOCKET_NB) + '=' +
                                      std::to_string(g_shell.shournalSocketNb);
    args.push_back(shournalSocketNbStr.c_str());

    for(char* const *e = envp; *e != nullptr; e++) {
        args.push_back(*e);
    }
    // optimization in shournal-run...
    args.push_back("SHOURNAL_DUMMY_NULL=1");
    std::string envSize = std::to_string(args.size() - envSizeIdx - 1);
    args[envSizeIdx] = envSize.c_str();

    args.push_back("--exec-filename");
    args.push_back(filename);

    args.push_back("--exec");
    fullCmd += filenameStr + ' ';

    for(int i=0; ; i++) {
        // include final nullptr here
        args.push_back(argv[i]);
        if(argv[i] == nullptr){
            break;
        }
        if(i > 0){
            // for the ignore-list skip argv0 which should be the same
            // as filename in most cases anyway.
            fullCmd.append(argv[i]);
            fullCmd += ' ';
        }
    }
    // strip final whitespace
    fullCmd.pop_back();
    if(sets.ignoreCmds().find(fullCmd) !=
            sets.ignoreCmds().end()){
        logDebug << "exec UNobserved:" << fullCmd.c_str();
        return execveUnobserved(filename, argv, envp);
    }

    logDebug << "execvpe observed:" << fullCmd.c_str();
    try {
        os::exec(args, envp);
    } catch (const os::ExcOs& e) {
        logCritical << qtr("Failed to launch %1 with external program. "
                           "Please make sure %2 is in your PATH: %3. "
                           "Running it unobserved instead...")
                       .arg(filename, app::SHOURNAL_RUN_FANOTIFY, e.what());
    }
    return execveUnobserved(filename, argv, envp);
}



