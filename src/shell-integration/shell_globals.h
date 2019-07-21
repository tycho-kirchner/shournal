#pragma once

#include <signal.h>
#include <sched.h>
#include <atomic>
#include <QByteArray>
#include <QDateTime>
#include <mutex>

#include "os.h"
#include "fdcommunication.h"
#include "sessioninfo.h"
#include "attached_shell.h"

typedef pid_t (*fork_func_t)();

typedef int (*execve_func_t)(const char *filename, char *const argv[],
                  char *const envp[]);

typedef int (*open_func_t)(const char *pathname, int flags, mode_t mode);

// typedef FILE* (*fopen_func_t)(const char *path, const char *mode);


enum class E_WatchState {DISABLED, WITHIN_CMD, INTERMEDIATE, ENUM_END};


class ShellGlobals
{

public:
    static ShellGlobals& instance();

    int shournalSocketNb;
    fork_func_t orig_fork{};
    execve_func_t orig_execve{};
    open_func_t orig_open{};
    // fopen_func_t orig_fopen{};

    std::atomic_flag ignoreEvents{};

    E_WatchState watchState;
    bool inSubshell;
    fdcommunication::SocketCommunication shournalSocket;
    pid_t lastMountNamespacePid;

    struct sigaction origSigintAction{};
    std::atomic_flag ignoreSigation{};

    AttachedShell* pAttchedShell;
    QtMsgType verbosityLevel;
    int shournalSockFdDescripFlags;

    SessionInfo sessionInfo;
    int shournalRootDirFd;

public:
    ShellGlobals(const ShellGlobals &) = delete ;
    void operator=(const ShellGlobals &) = delete ;

private:
    ShellGlobals();

};


