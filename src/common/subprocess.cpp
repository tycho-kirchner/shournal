
#include <unistd.h>
#include <wait.h>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <cassert>
#include <string>
#include <fcntl.h>
#include <linux/limits.h>

#include "subprocess.h"
#include "os.h"
#include "osutil.h"
#include "excos.h"
#include "util.h"
#include "cleanupresource.h"
#include "fdentries.h"

using osutil::closeVerbose;

enum class LaunchMsgType { PID, EXCEPTION, ENUM_END };

struct LaunchMsg{
    LaunchMsgType msgType;
    int errorNumber; //errno
    pid_t pid;
};
static_assert (sizeof (LaunchMsg) <= PIPE_BUF, "LaunchMsg is too big..." );

static_assert (std::is_pod<LaunchMsg>(), "");


namespace  {


/// @throws ExcOs
bool readMsg(int fd, LaunchMsg* msg){
    ssize_t readN = os::read(fd, msg, sizeof (LaunchMsg));
    if(readN == 0){
        return false;
    }
    if(readN != sizeof (LaunchMsg)){
        // should never happen
        throw os::ExcOs("Failed to launch external process, "
                        "received invalid message from child.",
                        EINVAL);
    }
    return true;
}

[[noreturn]]
void throwFailedToLaunchEx(char *const argv[],
                                  const LaunchMsg & msg,
                                  const std::string& descrip=""){
    std::string desc_;
    if(descrip.empty()){
       desc_ = descrip;
    } else {
        desc_ = descrip + " - ";
    }
    throw os::ExcOs("Failed to launch external process <"
                    + argvToStr(argv) + "> -" + desc_,
                    msg.errorNumber);
}


/// convert to null-terminated vector which will be passed as argv.
std::vector<char*> toPointerVect(const subprocess::Args_t& args){
    std::vector<char*> pointerVec(args.size() + 1 ); // + 1 because of terminating NULL
    for(unsigned i = 0; i < args.size() ; ++i) {
        pointerVec[i] = const_cast<char*>(args[i].c_str());
    }
    pointerVec.back() = nullptr;
    return pointerVec;
}

} // namespace

subprocess::Subprocess::Subprocess() :
    m_lastPid(std::numeric_limits<pid_t>::max()),
    m_asRealUser(false),
    m_forwardAllFds(false),
    m_lastCallWasDetached(false),
    m_environ(nullptr),
    m_inNewSid(false),
    m_waitForSetup(true),
    m_lastCallWaitedForSetup(false)
{}

void subprocess::Subprocess::call(char *const argv[],
                                  bool forwardStdin, bool forwardStdout, bool forwardStderr)
{
    this->call(argv[0], argv, forwardStdin, forwardStdout, forwardStderr);
}


void subprocess::Subprocess::call(const Args_t &args, bool forwardStdin,
                                  bool forwardStdout, bool forwardStderr)
{

    this->call(toPointerVect(args).data(), forwardStdin, forwardStdout, forwardStderr);
}

/// Call provided program after fork. Note that per default all file descriptors
/// except stdin, stdout and stderr are closed.
/// @throw ExcOs
void subprocess::Subprocess::call(const char *filename,
                                  char * const argv[], bool forwardStdin,
                                  bool forwardStdout, bool forwardStderr)
{
    doCall(filename, argv, forwardStdin, forwardStdout, forwardStderr,
           false);
}

/// Call provided program after double-fork (daemonize).
/// Waits, until grandchild-process. Note that per default all file descriptors
/// except stdout and stderr are closed.
/// @throw ExcOs
void subprocess::Subprocess::callDetached(const char *filename, char * const argv[],
                                          bool forwardStdin, bool forwardStdout,
                                          bool forwardStderr)
{
   doCall(filename, argv, forwardStdin, forwardStdout, forwardStderr,
          true);
}


void subprocess::Subprocess::callDetached(char * const argv[], bool forwardStdin,
                                          bool forwardStdout, bool forwardStderr)
{
   callDetached(argv[0], argv, forwardStdin, forwardStdout, forwardStderr);
}


void subprocess::Subprocess::callDetached(const Args_t &args, bool forwardStdin,
                                          bool forwardStdout, bool forwardStderr)
{
    this->callDetached(toPointerVect(args).data(), forwardStdin, forwardStdout, forwardStderr);
}


void subprocess::Subprocess::
doCall(const char *filename, char * const argv[],
       bool forwardStdin, bool forwardStdout,
       bool forwardStderr, bool detached)
{
    m_lastCallWasDetached = detached;
    m_lastCallWaitedForSetup = m_waitForSetup;
    if(m_waitForSetup){
        doCallWaitForSetup(filename, argv, forwardStdin, forwardStdout, forwardStderr,
                   detached);
    } else {
        os::Pipes_t dummyPipe {-1, -1};
        doFork(filename, argv, forwardStdin, forwardStdout, forwardStderr,
               detached, dummyPipe);
    }
}

/// Create a pipe and wait until the child-process has closed it's
/// write-end, either by calling execve or on error.
/// In case of a detached call, the pid of the grandchild-process
/// is also send via the pipe and made available via m_lastPid.
void subprocess::Subprocess::
doCallWaitForSetup(const char *filename, char * const argv[],
           bool forwardStdin, bool forwardStdout,
           bool forwardStderr, bool detached){
    auto startPipe = os::pipe( O_CLOEXEC  | O_DIRECT );
    auto closeStartRead = finally([&startPipe] {
        closeVerbose(startPipe[0]);
    });
    auto closeStartWrite = finally([&startPipe] {
        closeVerbose(startPipe[1]);
    });

    doFork(filename, argv, forwardStdin, forwardStdout, forwardStderr,
           detached, startPipe);

    closeVerbose(startPipe[1]);
    closeStartWrite.setEnabled(false);
    LaunchMsg msg;

    if(! detached){
        if(!readMsg(startPipe[0], &msg)){
            // no error
            return;
        }
        assert(msg.msgType == LaunchMsgType::EXCEPTION);
        throwFailedToLaunchEx(argv, msg);
    }

    // if detached we need the grandchild-pid

    if(!readMsg(startPipe[0], &msg)){
        // first message *must* be pid
       throwFailedToLaunchEx(argv, msg, "Missing pid reply from grandchild");
    }

    switch (msg.msgType) {
    case LaunchMsgType::PID:
        // normal case: pid of grandchild
        m_lastPid = msg.pid;
        break;
    case LaunchMsgType::EXCEPTION:
       throwFailedToLaunchEx(argv, msg);
    default:
        assert(false);
        throwFailedToLaunchEx(argv, msg,
                               " Bad response from grandchild: "
                              + std::to_string(int(msg.msgType)));

    }
    // second reply (if any) must be an exception always
    if(!readMsg(startPipe[0], &msg)){
        return;
    }
    assert(msg.msgType == LaunchMsgType::EXCEPTION);
    throwFailedToLaunchEx(argv, msg);
}


void subprocess::Subprocess::
doFork(const char *filename, char * const argv[],
           bool forwardStdin, bool forwardStdout,
           bool forwardStderr, bool detached,
           os::Pipes_t &startPipe){

    m_lastPid = os::fork();
    if(m_lastPid == 0){
        if(m_inNewSid)
            os::setsid();

        if(detached){
            // child: fork again and exit
            try {
                pid_t pid2 = os::fork();
                if(pid2 == 0){
                    handleChild(filename, argv, startPipe,
                                forwardStdin, forwardStdout, forwardStderr);
                }
            } catch (const os::ExcOs& ex){
                // should never happen
                std::cerr << __func__ << ": " << ex.what() << "\n";
                exit(1);
            }
            exit(0);
        } else {
            handleChild(filename, argv, startPipe,
                        forwardStdin, forwardStdout, forwardStderr);
        }
    }

}


/// Wait for the subprocess to finish. Does *not* work
/// for detached process
/// @return the exit value of the process
/// @throws ExcOs, ExcProcessExitNotNormal
int subprocess::Subprocess::waitFinish()
{
    if(m_lastCallWasDetached){
        throw os::ExcOs("Attempted to wait for child process, "
                        "although last call was <detached>", 0);
    }
    int child_status = 1;
    os::waitpid (m_lastPid, &child_status) ;
    return child_status;
}

void subprocess::Subprocess::setAsRealUser(bool val)
{
    m_asRealUser = val;
}

/// Per default all file-descriptors are closed, except
/// for "nomal" call:
///         stdin, stdout and stderr.
/// for detached call:
///         stdout and stderr
/// With this method you override the default.
void subprocess::Subprocess::setForwardFdsOnExec(const std::unordered_set<int> &forwardFds)
{
    m_forwardFds = forwardFds;
}

void subprocess::Subprocess::setForwardAllFds(bool val)
{
    m_forwardAllFds = val;
}

void subprocess::Subprocess::setInNewSid(bool val)
{
    m_inNewSid = val;
}

/// Wait for child-process-setup on call or callDetached.
void subprocess::Subprocess::setWaitForSetup(bool waitForSetup)
{
    m_waitForSetup = waitForSetup;
}

void subprocess::Subprocess::closeAllButForwardFds(os::Pipes_t &startPipe)
{
    // startpipe fds have O_CLOEXEC set, if exec fails, the respond is sent via
    // them, so do not close here.

    for(const int fd : osutil::FdEntries()){
        if(fd <= 2){
            // stdin, -out and -err are handeled separately
            continue;
        }
        if(m_forwardFds.find(fd) == m_forwardFds.end() &&
                fd != startPipe[0] && fd != startPipe[1])
            // not in white-list, close
            closeVerbose(fd);
    }
}

void subprocess::Subprocess::handleChild(const char *filename, char *const argv[],
                                         os::Pipes_t &startPipe, bool forwardStdin,
                                         bool forwardStdout, bool forwardStderr)
{    
    try {
        if(m_callbackAsChild)
            m_callbackAsChild();

        if(m_asRealUser){
            os::setgid(os::getgid());
            os::setuid(os::getuid());
        }
        if(startPipe[0] != -1 && m_lastCallWasDetached){
            LaunchMsg msg{};
            msg.msgType = LaunchMsgType::PID;
            msg.pid = getpid();
            os::write(startPipe[1], &msg, sizeof (LaunchMsg));
        }        
        if(! m_forwardAllFds){
            if(! forwardStdin) closeVerbose(STDIN_FILENO);
            if(! forwardStdout) closeVerbose(STDOUT_FILENO);
            if(! forwardStderr) closeVerbose(STDERR_FILENO);
            closeAllButForwardFds(startPipe);
        }
        os::exec(filename, argv, m_environ);
    } catch (const os::ExcOs& ex) {
        if(startPipe[0] == -1){
            std::cerr << "Failed to launch subprocess: "
                      << ex.what() << "\n";
            exit(ex.errorNumber());
        }
        LaunchMsg msg{};
        msg.msgType = LaunchMsgType::EXCEPTION;
        msg.errorNumber = ex.errorNumber();
        try {
            os::write(startPipe[1], &msg, sizeof (LaunchMsg));
        } catch (const os::ExcOs& ex) {
            // should never happen
            std::cerr << __func__ << ": " << ex.what() << "\n";
        }
        exit(ex.errorNumber());
    }
}


void subprocess::Subprocess::setEnviron(char **env)
{
    m_environ = env;
}

void subprocess::Subprocess::setCallbackAsChild
(const std::function<void ()> &callbackAsChild)
{
    m_callbackAsChild = callbackAsChild;
}


/// In case of callDetached the grandchild-PID is returned (
/// but only if we waited for setup, else the pid is invalid!).
pid_t subprocess::Subprocess::lastPid() const
{
    if(m_lastCallWasDetached && ! m_lastCallWaitedForSetup){
        throw QExcProgramming("m_lastCallWasDetached && ! m_lastCallWaitedForSetup");
    }

    return m_lastPid;
}
