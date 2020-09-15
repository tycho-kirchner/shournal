
#include <cassert>
#include <sys/types.h>
#include <csignal>
#include <poll.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <linux/securebits.h>

#include <QHostInfo>
#include <QDir>

#include <thread>
#include <future>

#include "filewatcher.h"
#include "fanotify_controller.h"
#include "mount_controller.h"
#include "os.h"
#include "osutil.h"
#include "oscaps.h"
#include "cleanupresource.h"
#include "fdcommunication.h"
#include "util.h"
#include "logger.h"
#include "subprocess.h"
#include "excos.h"
#include "db_globals.h"
#include "db_connection.h"
#include "db_controller.h"
#include "commandinfo.h"
#include "translation.h"
#include "subprocess.h"
#include "app.h"
#include "pathtree.h"
#include "fileeventhandler.h"
#include "orig_mountspace_process.h"
#include "cpp_exit.h"
#include "qfilethrow.h"
#include "storedfiles.h"
#include "qoutstream.h"
#include "conversions.h"
#include "socket_message.h"
#include "stdiocpp.h"

using socket_message::E_SocketMsg;
using SocketMessages = fdcommunication::SocketCommunication::Messages;
using subprocess::Subprocess;
using osutil::closeVerbose;

const int PRIO_FANOTIFY_POLL = 2;
const int PRIO_DATABASE_FLUSH = 10;

static void unshareOrDie(){
    try {
        os::unshare( CLONE_NEWNS);
    } catch (const os::ExcOs& e) {
        logCritical << e.what();
        if(os::geteuid() != 0){
            logCritical << qtr("Note that the effective userid is not 0 (root), so most probably %1 "
                               "does not have the setuid-bit set. As root execute:\n"
                               "chown root %1 && chmod u+s %1").arg(app::SHOURNAL_RUN);
        }
        cpp_exit(1);
    }
}

/// Other applications unsharing their mount-namespace might rely on the
/// fact that they cannot be joined (except from root). Therefor shournal
/// allows only joining of processes whose (effective) gid matches
/// below group.
static gid_t findMsenterGidOrDie(){
    auto* groupInfo = getgrnam(app::MSENTER_ONLY_GROUP);
    if(groupInfo == nullptr){
        logCritical << qtr("group %1 does not exist on your "
                           "system but is required. Please add it:\n"
                           "groupadd %1").arg(app::MSENTER_ONLY_GROUP);
        cpp_exit(1);
    }
    return groupInfo->gr_gid;
}


/// The childprocess's mount-namespace can be joined by shournal-run (msenter).
/// It has a group-id which should be used solely for this purpose which
/// serves as a permission check, so shournal-run cannot be used to join
/// processes which were not 'created' by it.
FileWatcher::MsenterChildReturnValue FileWatcher::setupMsenterTargetChildProcess(){
    assert(os::geteuid() == os::getuid());
    os::seteuid(0);

    // set ids before fork, so parent does not need to wait for child
    // (msenter uid and gid permission check!)
    os::setegid(m_msenterGid);
    os::seteuid(m_realUid);

    auto pipe_ = os::pipe();
    auto msenterPid = os::fork();

    if(msenterPid != 0){
        // parent
        os::seteuid(0);
        os::setegid(os::getgid());
        os::seteuid(m_realUid);
        os::close(pipe_[0]);
        return {msenterPid, pipe_[1]};
    }
    // child
    if(m_sockFd != -1){
        // the socket is used to wait for other processes, not this one, so:
        os::close(m_sockFd);
    }
    os::close(pipe_[1]);
    char c;
    // wait unitl parent-process closes its write-end
    os::read(pipe_[0], &c, 1);
    exit(0);
}

FileWatcher::FileWatcher() :
    m_sockFd(-1),
    m_msenterGid(std::numeric_limits<gid_t>::max()),
    m_commandArgc(0),
    m_commandFilename(nullptr),
    m_commandArgv(nullptr),
    m_commandEnvp(environ),
    m_realUid(os::getuid()),
    m_storeToDatabase(true)
{}

void FileWatcher::setupShellLogger()
{
    m_shellLogger.setFullpath(logger::logDir() + "/log_" + app::SHOURNAL + "_shell_integration");
    m_shellLogger.setup();
}


/// Unshare the mount-namespace and mark the interesting mounts with fanotify according
/// to the paths specified in settings.
/// Then either start a new process (passed argv) or wait until the passed socket is closed.
/// In this case, we are in the shell observation mode.
/// To allow other processes to join (--msenter), we fork off a child process with a
/// special group id, which waits for us to finish.
/// Process fanotify events until the observed process finishes (first case) or until
/// all other instances of the passed socket are closed by the observed processes.
/// See also code in directory 'shell-integration'.
void FileWatcher::run()
{
    m_msenterGid = findMsenterGidOrDie();
    orig_mountspace_process::setupIfNotExist();

    unshareOrDie();    
    auto fanotifyCtrl = FanotifyController_ptr(new FanotifyController);

    // We process events (filedescriptor-receive- and fanotify-events) with the
    // effective uid of the caller, because read events for files, for which
    // only the owner has read permission, usually fail for
    // root in case of NFS-storages. See also man 5 exports, look for 'root squashing'.
    os::seteuid(m_realUid);
    // fevent-handler sets up a temp dir in constructor - use user privileged
    m_fEventHandler = std::make_shared<FileEventHandler>();
    fanotifyCtrl->setFileEventHandler(m_fEventHandler);
    fanotifyCtrl->setupPaths();

    // We are indeed a non-interactive SCHED_BATCH-job
    struct sched_param sched{};
    sched.sched_priority = 0;
    if(sched_setscheduler(getpid(), SCHED_BATCH | SCHED_RESET_ON_FORK, &sched) == -1){
        logInfo << __FILE__ << "sched_setscheduler failed" << translation::strerror_l(errno) ;
    }

    CommandInfo cmdInfo =  CommandInfo::fromLocalEnv();
    cmdInfo.sessionInfo.uuid = m_shellSessionUUID;

    int ret = 1;
    m_sockCom.setReceiveBufferSize(RECEIVE_BUF_SIZE);
    E_SocketMsg pollResult;
    if(m_commandArgc != 0){
        if(m_commandFilename != nullptr){
            cmdInfo.text += QString(m_commandFilename) + " ";
        }
        cmdInfo.text += argvToQStr(m_commandArgc, m_commandArgv);
        auto sockPair = os::socketpair(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC );
        m_sockCom.setSockFd(sockPair[0]);

        Subprocess proc;
        proc.setAsRealUser(true);
        proc.setEnviron(m_commandEnvp);
        cmdInfo.startTime = QDateTime::currentDateTime();
        // TOODO: evtl. allow to configure proc to not close one of our sockets,
        // to wait on grandchildren.
        // Remove SOCK_CLOEXEC for one of them in that case
        const char* cmdFilename = (m_commandFilename == nullptr) ? m_commandArgv[0]
                                                                 : m_commandFilename;
        proc.call(cmdFilename, m_commandArgv);
        // *Must* be called after fork (resurce limits, etc.)
        std::future<E_SocketMsg> thread = std::async(&FileWatcher::pollUntilStopped, this,
                                                     std::ref(cmdInfo),
                                                     std::ref(fanotifyCtrl));
        try {
            cmdInfo.returnVal = proc.waitFinish();
        } catch (const os::ExcProcessExitNotNormal& ex) {
            // return typical shell cpp_exit code
            cmdInfo.returnVal = 128 + ex.status();
        }
        ret = cmdInfo.returnVal;
        // that should stop the polling event loop:
        os::close(sockPair[1]);
        thread.wait();
        os::close(sockPair[0]);
        pollResult = thread.get();
    } else if(m_sockFd != -1){
        MsenterChildReturnValue msenterChildRet = setupMsenterTargetChildProcess();
        auto closeMsenterWritePipe = finally([&msenterChildRet] {
            os::close(msenterChildRet.pipeWriteEnd);
            os::waitpid(msenterChildRet.pid);
        });
        m_sockCom.setSockFd(m_sockFd);
        // should be overwritten later, null-constraint in db...
        cmdInfo.startTime = QDateTime::currentDateTime();
        setupShellLogger();
        int rootDirFd = os::open("/", O_RDONLY | O_DIRECTORY);
        auto closeRootDir = finally([&rootDirFd] { closeVerbose(rootDirFd);} );
        SocketMessages sockMesgs;
        m_sockCom.sendMsg({int(E_SocketMsg::SETUP_DONE),
                           qBytesFromVar(msenterChildRet.pid), rootDirFd});

        pollResult = pollUntilStopped(cmdInfo, fanotifyCtrl);
        ret = 0;
    } else {
        pollResult = E_SocketMsg::ENUM_END;
        assert(false);
    }

    cmdInfo.endTime = QDateTime::currentDateTime();
    logDebug << "polling finished - about to cleanup and exit";

    fanotifyCtrl.reset();

    switch (pollResult) {
    case E_SocketMsg::EMPTY: break; // Normal case
    case E_SocketMsg::ENUM_END:
        logCritical << qtr("Because an error occurred, processing of "
                           "fanotify/socket-events was "
                            "stopped");
        cpp_exit(ret);
    default:
        logWarning << "unhandled case for pollResult: " << int(pollResult);
        break;
    }

    QStringList missingFields;
    if(cmdInfo.text.isEmpty()){
        // An empty command text should only occur, if the observed shell-session
        // exits. Discard this command.
        logDebug << "command-text is empty, "
                    "not pushing to database...";
        cpp_exit(ret);
    }
    if(cmdInfo.returnVal == CommandInfo::INVALID_RETURN_VAL){
        missingFields += qtr("return value");
    }
    if(! missingFields.isEmpty()){
        logDebug << "The following fields are empty: " << missingFields.join(", ");
    }

    if(m_storeToDatabase){
        flushToDisk(cmdInfo);
    }
    cpp_exit(ret);
}

void FileWatcher::setShellSessionUUID(const QByteArray &shellSessionUUID)
{
    m_shellSessionUUID = shellSessionUUID;
}

void FileWatcher::setArgv(char **argv, int argc)
{
    m_commandArgv = argv;
    m_commandArgc = argc;
}

void FileWatcher::setCommandEnvp(char **commandEnv)
{
    m_commandEnvp = commandEnv;
}

void FileWatcher::setSockFd(int sockFd)
{
    m_sockFd = sockFd;
}

int FileWatcher::sockFd() const
{
    return m_sockFd;
}

void FileWatcher::setStoreToDatabase(bool storeToDatabase)
{
    m_storeToDatabase = storeToDatabase;
}

void FileWatcher::setCommandFilename(char *commandFilename)
{
    m_commandFilename = commandFilename;
}


///  @return E_SocketMsg::EMPTY, if processing shall be stopped
E_SocketMsg FileWatcher::processSocketEvent( CommandInfo& cmdInfo ){
    m_sockCom.receiveMessages(&m_sockMessages);
    E_SocketMsg returnMsg = E_SocketMsg::ENUM_END;
    for(auto & msg : m_sockMessages){
        if(msg.bytes.size() > RECEIVE_BUF_SIZE - 1024*10){
            logWarning << "unusual large message received";
        }
        if(msg.msgId == -1){
            return E_SocketMsg::EMPTY;
        }
        assert(msg.msgId >=0 && msg.msgId < int(E_SocketMsg::ENUM_END));

        returnMsg = E_SocketMsg(msg.msgId);

        logDebug << "received message:"
                 << socket_message::socketMsgToStr(E_SocketMsg(msg.msgId))
                 << msg.bytes;
        switch (E_SocketMsg(msg.msgId)) {
        case E_SocketMsg::COMMAND: {
            cmdInfo.text = msg.bytes;
            break;
        }
        case E_SocketMsg::CMD_START_DATETIME: {
            cmdInfo.startTime = QDateTime::fromString(QString::fromUtf8(msg.bytes),
                                                      Conversions::dateIsoFormatWithMilliseconds());
            assert(! cmdInfo.startTime.isNull());
            break;
        }
        case E_SocketMsg::RETURN_VALUE: {
            cmdInfo.returnVal = varFromQBytes<qint32>(msg.bytes);
            break;
        }
        case E_SocketMsg::LOG_MESSAGE:
            m_shellLogger.stream() << msg.bytes << endl;
            break;

        case E_SocketMsg::CLEAR_EVENTS:
            m_fEventHandler->clearEvents();
            cmdInfo.startTime = QDateTime::currentDateTime();
            break;
        default: {
            // application bug?
            returnMsg = E_SocketMsg::EMPTY;
            logCritical << qtr("invalid message received - : %1").arg(int(msg.msgId));
            break;
        }
        }
    }
    assert(returnMsg != E_SocketMsg::ENUM_END);
    return returnMsg;
}

void FileWatcher::flushToDisk(CommandInfo& cmdInfo){
    assert(os::getegid() == os::getgid());
    assert(os::geteuid() == os::getuid());

    // Do not disturb other processes while we flush events to database
    os::setpriority(PRIO_PROCESS, 0, PRIO_DATABASE_FLUSH);
    try {
        cmdInfo.idInDb = db_controller::addCommand(cmdInfo);
        StoredFiles::mkpath();
        m_fEventHandler->writeEvents().fseekToBegin();
        m_fEventHandler->readEvents().fseekToBegin();

        db_controller::addFileEvents(cmdInfo, m_fEventHandler->writeEvents(),
                                     m_fEventHandler->readEvents() );
    } catch (std::exception& e) {
        // May happen, e.g. if we run out of disk space...
        // We discard events anyway, so this error will not happen too soon again...
        logCritical << qtr("Failed to store (some) file-events to disk: %1").arg(e.what());
    }
}



/// @return: EMPTY, if stopped regulary
///          ENUM_END in case of an error
E_SocketMsg FileWatcher::pollUntilStopped(CommandInfo& cmdInfo,
                             FanotifyController_ptr& fanotifyCtrl){

    // To allow for more fanotify-events read at a time, increase
    // RLIMIT_NOFILE
    struct rlimit rlim{};
    getrlimit(RLIMIT_NOFILE, &rlim);
    const auto NO_FILE = fanotifyCtrl->getFanotifyMaxEventCount();
    rlim.rlim_cur = NO_FILE;
    if(setrlimit(RLIMIT_NOFILE, &rlim) == -1){
        logInfo << qtr("Failed to set number of open files to %1 - %2")
                   .arg(NO_FILE)
                   .arg(translation::strerror_l(errno));
    }

    // At least on centos 7 with Kernel 3.10 CAP_SYS_PTRACE is required, otherwise
    // EACCES occurs on readlink of the received file descriptors
    // Warning: changing euid from 0 to nonzero resets the effective capabilities,
    // so don't do that until processing finished.
    auto caps = os::Capabilites::fromProc();
    const os::Capabilites::CapFlags eventProcessingCaps { CAP_SYS_PTRACE, CAP_SYS_NICE };
    caps->setFlags(CAP_EFFECTIVE, { eventProcessingCaps });
    auto resetEventProcessingCaps = finally([&caps, &eventProcessingCaps] {
        caps->clearFlags(CAP_EFFECTIVE, eventProcessingCaps);
    });

    os::setpriority(PRIO_PROCESS, 0, PRIO_FANOTIFY_POLL);
    auto resetPriority = finally([] {
        os::setpriority(PRIO_PROCESS, 0, 0);
    });

    int poll_num;
    const nfds_t nfds = 2;
    struct pollfd fds[nfds];

    fds[0].fd = m_sockCom.sockFd();
    fds[0].events = POLLIN;

    // Fanotify input
    fds[1].fd = fanotifyCtrl->fanFd();
    fds[1].events = POLLIN;
    while (true) {
        // cleanly cpp_exit poll:
        // poll for two file descriptors: the fanotify descriptor and
        // another one, which receives an cpp_exit-message).
        poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR){     // Interrupted by a signal
                continue;            // Restart poll()
            }
            logCritical << qtr("poll failed (%1) - %2").arg(errno)
                           .arg(translation::strerror_l());
            return E_SocketMsg::ENUM_END;
        }
        // 0 only on timeout, which is infinite
        assert(poll_num != 0);

        // Important: first handle fanotify events, then check the socket if we are done.
        // Otherwise final fanotify-events might get lost!
        if (fds[1].revents & POLLIN) {
            // Fanotify events are available
            logDebug << "new fanotify events...";
            fanotifyCtrl->handleEvents();
        }
        if (fds[0].revents & POLLIN) {
            if(processSocketEvent(cmdInfo) == E_SocketMsg::EMPTY){
                return E_SocketMsg::EMPTY;
            }
        }
    }

}






