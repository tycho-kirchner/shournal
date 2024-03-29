#pragma once

#include <csignal>
#include <sched.h>
#include <atomic>
#include <QByteArray>
#include <QDateTime>
#include <mutex>

#include "attached_shell.h"
#include "fdcommunication.h"
#include "logger.h"
#include "os.h"
#include "sessioninfo.h"
#include "util.h"


typedef pid_t (*fork_func_t)();

typedef int (*execve_func_t)(const char *filename, char *const argv[],
                  char *const envp[]);

typedef int (*open_func_t)(const char *pathname, int flags, mode_t mode);
typedef char * (*strcpy_func_t)(char *, const char*);

extern const char* ENV_VARNAME_SHELL_VERBOSITY;

enum class E_WatchState {DISABLED, WITHIN_CMD, INTERMEDIATE, ENUM_END};


class ShellGlobals
{

public:
    static ShellGlobals& instance();
    static bool performBasicInitIfNeeded();
    static bool updateVerbosityFromEnv(bool verboseIfUnset);

    int shournalSocketNb {-1};
    fork_func_t orig_fork {};
    execve_func_t orig_execve {};
    open_func_t orig_open {};
    strcpy_func_t orig_strcpy {};

    std::atomic_flag ignoreEvents{};

    E_WatchState watchState {E_WatchState::DISABLED};
    bool inParentShell {false};
    fdcommunication::SocketCommunication shournalSocket;
    pid_t lastMountNamespacePid {-1};

    struct sigaction origSigintAction{};
    std::atomic_flag ignoreSigation{};

    AttachedShell* pAttchedShell {};
    QtMsgType verbosityLevel {QtMsgType::QtWarningMsg};
    std::string shournalRunVerbosity {logger::msgTypeToStr(QtWarningMsg)};
    int shournalSockFdDescripFlags {-1};

    SessionInfo sessionInfo;
    int shournalRootDirFd {-1};

    pid_t shellParentPid {0};

public:
    ~ShellGlobals() = default;
    Q_DISABLE_COPY(ShellGlobals)
    DISABLE_MOVE(ShellGlobals)

private:
    ShellGlobals();

};


