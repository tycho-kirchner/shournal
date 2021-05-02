#pragma once

#include <unordered_set>

#include "os.h"



namespace subprocess {

typedef std::vector<std::string> Args_t;

/// Call external programs via fork and exec
/// and wait for it to finish later
class Subprocess {
public:

    Subprocess();

    void call(char *const argv[],
              bool forwardStdin=true,
              bool forwardStdout=true,
              bool forwardStderr=true);

    void call(const Args_t &args, bool forwardStdin=true,
                                  bool forwardStdout=true,
                                  bool forwardStderr=true);

    void call(const char *filename, char * const argv[],
              bool forwardStdin=true,
              bool forwardStdout=true,
              bool forwardStderr=true);

    void callDetached(char *const argv[], bool forwardStdin=false,
                      bool forwardStdout=true,
                      bool forwardStderr=true);

    void callDetached(const char *filename, char *const argv[], bool forwardStdin=false,
                      bool forwardStdout=true,
                      bool forwardStderr=true);

    void callDetached(const Args_t &args, bool forwardStdin=false,
                      bool forwardStdout=true,
                      bool forwardStderr=true);

    int waitFinish();

    void setAsRealUser(bool val);
    void setForwardFdsOnExec(const std::unordered_set<int>& forwardFds);
    void setForwardAllFds(bool val);
    void setInNewSid(bool val);
    void setWaitForSetup(bool waitForSetup);

    pid_t lastPid() const;
    void setEnviron(char **env);

    void setCallbackAsChild(const std::function<void ()> &callbackAsChild);


private:
    void closeAllButForwardFds(os::Pipes_t &startPipe);
    [[noreturn]]
    void handleChild(const char *filename, char * const argv[], os::Pipes_t & startPipe,
                     bool forwardStdin, bool forwardStdout, bool forwardStderr);
    void doCall(const char *filename, char * const argv[],
                    bool forwardStdin,
                    bool forwardStdout,
                    bool forwardStderr,
                    bool detached);
    void doCallWaitForSetup(const char *filename, char * const argv[],
                    bool forwardStdin,
                    bool forwardStdout,
                    bool forwardStderr,
                    bool detached);
    void doFork(const char *filename, char * const argv[],
                    bool forwardStdin,
                    bool forwardStdout,
                    bool forwardStderr,
                    bool detached,
                    os::Pipes_t &startPipe);


    pid_t m_lastPid;
    bool m_asRealUser;
    std::unordered_set<int> m_forwardFds;
    bool m_forwardAllFds;
    bool m_lastCallWasDetached;
    char** m_environ;
    bool m_inNewSid;
    bool m_waitForSetup;
    bool m_lastCallWaitedForSetup;
    std::function< void()> m_callbackAsChild;
};


} // namespace subprocess
