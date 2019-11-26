
#include <sys/socket.h>
#include <cassert>
#include <cstdlib>
#include <QCoreApplication>
#include <dirent.h>

#include "shell_request_handler.h"

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
#include "attached_bash.h"
#include "staticinitializer.h"
#include "socket_message.h"
#include "interrupt_handler.h"
#include "conversions.h"

using socket_message::E_SocketMsg;
using socket_message::socketMsgToStr;
using fdcommunication::SocketCommunication;


namespace  {



/// ENABLE: shell observation enabled
/// DISABLE: shell observation disabled
/// PREPARE_CMD: prepare observing the next command-sequence
/// CLEANUP_CMD: stop monitoring the command-sequence and send command-info to external shournal
/// PRINT_VERSION: print the version of *this* shared library
/// DUMMY: no action is taken, except to unset the trigger env.-variable, which
/// can be used to check, whether this .so is loaded.
enum class ShellRequest {ENABLE, DISABLE,
                         PREPARE_CMD, CLEANUP_CMD,
                         PRINT_VERSION,
                         DUMMY,
                         ENUM_END};



// const char* shellRequestToStr(ShellRequest r){
//     switch (r) {
//     case ShellRequest::ENABLE:
//         return "enable";
//     case ShellRequest::DISABLE:
//         return "disable";
//     case ShellRequest::PREPARE_CMD:
//         return "prepare_cmd";
//     case ShellRequest::CLEANUP_CMD:
//         return "cleanup_cmd";
//     case ShellRequest::PRINT_VERSION:
//         return "print_version";
//     case ShellRequest::SIGINT_HANDLER_INSTALL:
//         return "sigint_install";
//     case ShellRequest::SIGINT_HANDLER_RESTORE:
//         return "sigint_restore";
//     case ShellRequest::DUMMY:
//         return "dummy";
//     case ShellRequest::ENUM_END:
//         return "enumend!";
//     }
//     return "unkown";
// }


bool initializeAttachedShellIfNeeded(){
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.pAttchedShell != nullptr){
        return true;
    }
    const char* attachedShellName = getenv("_SHOURNAL_SHELL_NAME");
    if(attachedShellName == nullptr){
        // should never happen
        logCritical << "shell name is not set in envrionment. Command not reported.";
        return false;
    }
    switch (attachedShellName[0]) {
    case 'b':
        try {
            g_shell.pAttchedShell = new AttachedBash();
        } catch (const os::ExcOs& e) {
            logCritical << "Failed to initialize attached shell:" << e.what();
            return false;
        }
        break;
    default:
        logCritical << "unknown shell name:" << attachedShellName;
        return false;
    }
    return true;
}



bool loadSettings(){
    try {
        // maybe_todo: copy file to another path and load the same file later in shournal-run:
        Settings::instance().load();
        return true;
    } catch (const std::exception& e) {
        logCritical << e.what() << "\n";
    }
    logCritical << "Because of that, the shell observation is disabled\n";
    return false;
}


void updateVerbosityFromEnv(){
    const char* desiredVerbosityStr = getenv("_SHOURNAL_LIB_SHELL_VERBOSITY");
    if(desiredVerbosityStr == nullptr){
        return;
    }
    if(strlen(desiredVerbosityStr) == 0){
        logWarning << qtr("Verbosity environment variable '_SHOURNAL_LIB_SHELL_VERBOSITY' "
                          "is set to an empty string");
        return;
    }

    auto& g_shell = ShellGlobals::instance();
    g_shell.verbosityLevel = logger::strToMsgType(desiredVerbosityStr);

}


/// Shells usually start at low numbers for internal file descriptors (usually 10),
/// we try to find the highest possible free fd
/// If startFd != -1, start searching from that.
int verbose_findHighestFreeFd(int startFd=-1){
    int fd = osutil::findHighestFreeFd(startFd, 30);
    if(fd == -1){
        logWarning << qtr("Could not find a free file descriptor number. "
                          "The max. number of open files for this process is %1.")
                      .arg(osutil::getMaxCountOpenFiles());
    }
    return fd;
}


/// Launch external shournal (detached) and wait for it to finish unsharing
/// the mount-NS and fanotify-marking the mounts. Since it is called in a new session
/// (setsid), it survives the parent shell (*this process), furhter it receives no sigint, destinated
/// for our shell, which could have caused it to terminate even before installing a SIGIGN-handler.
/// Pass a socket to shournal, which is used for communication *and* to stop it
/// (semi-)automatically. Each subsequentially launched process inherits it. Once
/// all of them finished *and* we cleaned up (or died), external shournal stops.
/// Note that for processes, which close passed file-descriptors
/// before exit, shournal might quit too early, in which case file modfication events
/// are lost
void handlePrepareCmd(){
    updateVerbosityFromEnv();
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.watchState == E_WatchState::WITHIN_CMD){
        // Happens e.g. if a previous cleanup request was ignored due to
        // an invalid command.
        logDebug << "Received setup-request while shell observation "
                                   "was already enabled (might be ok).";
        return;
    }

    if(! loadSettings()){
        return;
    }

    g_shell.shournalSocket.setSockFd(-1);

    try {
        g_shell.shournalSocketNb = verbose_findHighestFreeFd();
        if( g_shell.shournalSocketNb == -1){
            return;
        }
        g_shell.shournalRootDirFd = verbose_findHighestFreeFd(g_shell.shournalSocketNb - 1);
        if( g_shell.shournalRootDirFd == -1){
            return;
        }

        auto sockets = os::socketpair(PF_UNIX, SOCK_STREAM);

        auto autocloseSocket0 = finally([&sockets] { close(sockets[0]); });
        auto autocloseSocket1 = finally([&sockets] { close(sockets[1]); });


        subprocess::Args_t args = {
            app::SHOURNAL_RUN,
            "--socket-fd", std::to_string(sockets[0]),
            "--verbosity", logger::msgTypeToStr(g_shell.verbosityLevel),
            "--shell-session-uuid", g_shell.sessionInfo.uuid.toBase64().data()
        };


        g_shell.lastMountNamespacePid = -1;
        subprocess::Subprocess subproc;
        subproc.setInNewSid(true); // Survive parent shell exit
        // Pass the socket to the external shournal process for communication purposes.
        std::unordered_set<int> forwardFs {sockets[0]};
        if(app::inIntegrationTestMode()){
            // forward a pipe to async shournal so integration-test knows when it finished
            const char* pipeFdStr = getenv("_SHOURNAL_INTEGRATION_TEST_PIPE_FD");
            if(pipeFdStr == nullptr){
                QIErr() << "app is set to integration test mode, but pipe-fd is not set...";
            } else {
                int pipeFd = qVariantTo_throw<int>(QByteArray(pipeFdStr));
                if(! osutil::fdIsOpen(pipeFd)){
                    QIErr() << "_SHOURNAL_INTEGRATION_TEST_PIPE_FD set in env "
                               "but fd" << pipeFd <<  "is not open";
                } else {
                    forwardFs.insert(pipeFd);
                }
            }
        }

        subproc.setForwardFdsOnExec(forwardFs);
        subproc.call(args);

        os::close(sockets[0]);
        autocloseSocket0.setEnabled(false);

        // avoid deadlock: close our write end
        // wait for reply from shournal
        g_shell.shournalSocket.setReceiveBufferSize(100);
        g_shell.shournalSocket.setSockFd(sockets[1]);


        auto messages=g_shell.shournalSocket.receiveMessages();
        if(messages.size() != 1 ){
            logCritical << qtr("Setup of external %1-process failed: "
                               "expected one message but received %2")
                                .arg(app::SHOURNAL_RUN)
                                .arg(messages.size());
            return;
        }
        auto& socketMsg = messages.first();


        if( E_SocketMsg(socketMsg.msgId) != E_SocketMsg::SETUP_DONE){
            QString msg = (socketMsg.msgId < 0 || socketMsg.msgId >= int(E_SocketMsg::ENUM_END))
                                       ? qtr("Bad response")
                                       : socketMsgToStr(E_SocketMsg(socketMsg.msgId));

            logCritical << qtr("Setup of external %1-process failed, "
                               "received message: %2 (%3)")
                           .arg(app::SHOURNAL_RUN)
                           .arg(msg)
                           .arg(int(socketMsg.msgId));
            return;
        }

        g_shell.lastMountNamespacePid = varFromQBytes(socketMsg.bytes,
                                                      static_cast<pid_t>(-1));
        assert(socketMsg.fd != -1);

        if(socketMsg.fd != g_shell.shournalRootDirFd){
            os::dup2(socketMsg.fd, g_shell.shournalRootDirFd);
            os::close(socketMsg.fd);
        }
        auto RootDirFlags = os::getFdDescriptorFlags(g_shell.shournalRootDirFd);
        setBitIn(RootDirFlags, FD_CLOEXEC);
        os::setFdDescriptorFlags(g_shell.shournalRootDirFd, RootDirFlags);

        autocloseSocket1.setEnabled(false);
        if(sockets[1] != g_shell.shournalSocketNb ){
            // dup2 and close orig
            try {
                os::dup2(sockets[1], g_shell.shournalSocketNb);
                close(sockets[1]);
            } catch (const os::ExcOs& ex) {
                logCritical << "duplicating to shournal-wait-fd failed: "
                            << ex.what();
                close(sockets[1]);
                return;
            }
        }
        g_shell.shournalSocket.setSockFd(g_shell.shournalSocketNb);
        g_shell.shournalSockFdDescripFlags = os::getFdDescriptorFlags(g_shell.shournalSocketNb);

        g_shell.watchState = E_WatchState::WITHIN_CMD;
        shell_logger::flushBufferdMessages();

        return;
    } catch(const os::ExcOs& ex){
        logCritical << ex.what();
    } catch (const std::exception& e) {
        logCritical << "Unknown std::exception occurred: " << e.what() << "\n";
    } catch (...) {
        logCritical << "Unknown exception occurred\n";
    }
    g_shell.shournalSocket.setSockFd(-1);
    g_shell.shournalRootDirFd = -1;

}




/// Read update request from environment and check if the request
/// is valid (log error on exit).
/// @return The valid request, or ENUM_END if no request was found or it
/// was invalid.
ShellRequest readCheckShellUpdateRequest(){
    const char* TRIGGER_NAME = "_LIBSHOURNAL_TRIGGER";

    const char* shellStateStr = getenv(TRIGGER_NAME);
    if(shellStateStr == nullptr){
        // No update request
        return ShellRequest::ENUM_END;
    }

    auto unsetTrigger = finally([&TRIGGER_NAME] {
        try {
            os::unsetenv(TRIGGER_NAME);
        } catch (const os::ExcOs& e) {
            logCritical << e.what();
        }
    });

    uint shellRequestInt;
    try {
        qVariantTo_throw(shellStateStr, &shellRequestInt);
    } catch (const ExcQVariantConvert& ex) {
        logCritical << qtr("Cannot determine shell-request: ")
                    << ex.descrip();
        return ShellRequest::ENUM_END;
    }

    if(shellRequestInt >= static_cast<int>(ShellRequest::ENUM_END)){
        logCritical << qtr("Invalid shell-request passed:")
                    << shellRequestInt;
        return ShellRequest::ENUM_END;
    }
    auto shellRequest = static_cast<ShellRequest>(shellRequestInt);

    // Note: this logDebug is called BEFORE initialize logging.
    // logDebug << "received shell request:" << int(shellRequest);
    return shellRequest;
}

void verboseCloseShournalSocket(){
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.shournalSocket.sockFd() >= 0 &&
            close(g_shell.shournalSocket.sockFd()) == -1){
        logWarning << "close of shournal-socket failed:"
                   << translation::strerror_l();

    }
    g_shell.shournalSocket.setSockFd(-1);
}

void verboseCloseRootDirFd(){
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.shournalRootDirFd >= 0 &&
            close(g_shell.shournalRootDirFd) == -1){
        logWarning << "close of shournal-root dir-fd failed:"
                   << translation::strerror_l();

    }
    g_shell.shournalRootDirFd = -1;
}


void handleDisableRequest(){
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.watchState == E_WatchState::DISABLED){
        logWarning << qtr("Received disable-request while shell observation "
                          "was already disabled.");
        return;
    }
    g_shell.watchState = E_WatchState::DISABLED;

    verboseCloseShournalSocket();
    verboseCloseRootDirFd();
}


void handleCleanupCmd(){
    auto& g_shell = ShellGlobals::instance();

    auto clearCmdStartTime = finally([&g_shell] { g_shell.lastCmdStartTime = {}; });

    if(g_shell.lastCmdStartTime.isNull()){
        logDebug << "last command was invalid";

        // If, for example, a user presses enter while the command-string is empty, or Ctrl+C
        // is pressed without a currently executing command,
        // do not cleanup(setup) to avoid unnecessary overhead.
        // Clear possible events mich might have had occurred meanwhile, e.g. because
        // of autocompletion.
        if(g_shell.watchState == E_WatchState::WITHIN_CMD){
            g_shell.shournalSocket.sendMsg(int(E_SocketMsg::CLEAR_EVENTS));
        }
        return;
    }

    if(g_shell.watchState != E_WatchState::WITHIN_CMD){
        //   can happen, if
        // - being at the first prompt after SHOURNAL_ENABLE -> no command
        //   was observed yet, nothing to clean up
        // - an interrupt occurs: if a SIGINT occurred between the cleanup request and the installation
        //   of our sigint-handler or after cleanup but before the setup of the next command, nothing needs
        //   to be done.
        return;
    }

    auto finalActions = finally([&g_shell] {
        g_shell.watchState = E_WatchState::INTERMEDIATE;
        verboseCloseShournalSocket();
        verboseCloseRootDirFd();
    });

    QByteArray lastCommand = getenv("_SHOURNAL_LAST_COMMAND");
    if(lastCommand.isNull()){
        logWarning << "Failed to retrieve last command string from environment";
        lastCommand = "UNKNOWN";
    }    
    const char* lastReturnValueStr = getenv("_SHOURNAL_LAST_RETURN_VALUE");
    qint32 returnVal = CommandInfo::INVALID_RETURN_VAL;
    if(lastReturnValueStr == nullptr){
        logWarning << qtr("Failed to retrieve last return-value from environment");
    } else {
        try {
            qVariantTo_throw(lastReturnValueStr, &returnVal);
        } catch (const ExcQVariantConvert& ex) {
            logWarning << "Failed to convert last return value:"
                       << ex.descrip();
        }
    }
    SocketCommunication::Messages messages;
    messages.push_back({int(E_SocketMsg::COMMAND), lastCommand});
    const QByteArray cmdStartDateTime = g_shell.lastCmdStartTime.toString(
                Conversions::dateIsoFormatWithMilliseconds()).toUtf8();
    messages.push_back({int(E_SocketMsg::CMD_START_DATETIME), cmdStartDateTime});
    messages.push_back({int(E_SocketMsg::RETURN_VALUE), qBytesFromVar(returnVal)});
    g_shell.shournalSocket.sendMessages(messages);
}


void handleEnableRequest(){
    if(! initializeAttachedShellIfNeeded()){
        return;
    }

    updateVerbosityFromEnv();
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.watchState != E_WatchState::DISABLED){
        logDebug << "received enable request while watchstate != DISABLED"
                 << int(E_WatchState::DISABLED);
    }

    static StaticInitializer initOnFirstCall( [](){
        QIErr::setPreambleCallback([]() { return "shournal shell-integration: "; });
        app::setupNameAndVersion();
        try {
            if(! registerQtConversionStuff()){
                QIErr()  << qtr("Fatal error: failed to initialize custom Qt conversion functions");
            }
            shell_logger::setup();
            translation::init();
        } catch (const std::exception& ex) {
            logCritical << ex.what();
        }

        // This shell might have been launched within an already observed shell.
        // Note that when the shell observation was launched, we already left
        // the observerd mount namespace. Thus all that remains to do is closing
        // respective fd (which hopefully does not belong to another program..
        const char* socketNbStr = getenv(app::ENV_VAR_SOCKET_NB);
        if(socketNbStr != nullptr){
            auto laterUnsetIt = finally([] { unsetenv(app::ENV_VAR_SOCKET_NB); });

            int fdNb;
            try {
                qVariantTo_throw(socketNbStr, &fdNb);
            } catch (const ExcQVariantConvert& ex) {
                logCritical << qtr("Bad environment variable %1: ").arg(app::ENV_VAR_SOCKET_NB)
                            << ex.descrip();
                return;
            }
            if(osutil::fdIsOpen(fdNb)){
                logDebug << "initially closing shournal-socket" << fdNb;

                try {
                    os::close(fdNb);
                } catch (const os::ExcOs& e){
                    logCritical << "handleEnableRequest" << e.what();
                }
            } else {
                logInfo << QString("The environment variable %1 is set, but the socket "
                                    "%2 is not open").arg(app::ENV_VAR_SOCKET_NB).arg(fdNb);
            }
        }
    });

    g_shell.watchState = E_WatchState::INTERMEDIATE;
    bool madeSafe;
    g_shell.sessionInfo.uuid = make_uuid(&madeSafe);
    if(! madeSafe){
        logInfo << __func__ << qtr("session uuid not created 'safe'. Is the uuidd-daemon running?");
    }

    g_shell.pAttchedShell->handleEnable();
}


} // namespace



/// If the environment variable '_LIBSHOURNAL_TRIGGER' is set,
/// perform the set action (load settings, launch external shournal, etc.).
/// @return true, if a (valid) request occurred
bool shell_request_handler::checkForTriggerAndHandle(){
    ShellRequest request = readCheckShellUpdateRequest();
    if(request == ShellRequest::ENUM_END){
        return false;
    }

    // Interrupt protect mostly applies to wainting for a shournal response, which is
    // still short enough to justify not being interruptible.
    InterruptProtect ip;

    auto& g_shell = ShellGlobals::instance();

    if(g_shell.pAttchedShell == nullptr){
        // not initialized yet: only allow some requests:
        switch (request) {
        case ShellRequest::ENABLE:
        case ShellRequest::PRINT_VERSION:
        case ShellRequest::DUMMY:
            break;
        default:
            QIErr() << int(request) << "occurred, although the "
                                       "attached shell was not initialized (bug?)";
            return false;
        }
    }

    switch (request) {
    case ShellRequest::ENABLE:
        handleEnableRequest();
        break;
    case ShellRequest::DISABLE:
        handleDisableRequest();
        break;
    case ShellRequest::PREPARE_CMD:
        handlePrepareCmd();
        break;
    case ShellRequest::CLEANUP_CMD:
        handleCleanupCmd();
        break;
    case ShellRequest::PRINT_VERSION:
        QErr() << "libshournal-shellwatch.so version " << app::version().toString() << "\n";
        break;
    case ShellRequest::DUMMY:
        break;
    case ShellRequest::ENUM_END:
        break; // no event
    }
    // QIErr() << "new shell state:" << shellRequestToStr(request);
    return request != ShellRequest::ENUM_END;
}
