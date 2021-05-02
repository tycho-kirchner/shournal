
#include <QTemporaryFile>

#include "filewatcher_shournalk.h"

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#include "app.h"
#include "cefd.h"
#include "cpp_exit.h"
#include "conversions.h"
#include "commandinfo.h"
#include "db_controller.h"
#include "cleanupresource.h"
#include "fdentries.h"
#include "fifocom.h"
#include "fileevents.h"
#include "logger.h"
#include "mark_helper.h"
#include "os.h"
#include "osutil.h"
#include "qoutstream.h"
#include "settings.h"
#include "shournalk_ctrl.h"
#include "shournal_run_common.h"
#include "stdiocpp.h"
#include "subprocess.h"
#include "translation.h"


const int PRIO_DATABASE_FLUSH = 10;
const pid_t INVALID_PID = std::numeric_limits<pid_t>::max();


using subprocess::Subprocess;
using std::shared_ptr;
using std::make_shared;

using ShournalK_ptr = Filewatcher_shournalk::ShournalK_ptr;


static void handleFifoEvent(shared_ptr<FifoCom>& fifoCom,
                            CommandInfo* cmdInfo,
                            ShournalK_ptr& shournalk){
    enum { FIFO_RETURN_VAL=0, FIFO_UNMARK_PID};


    QString data;
    for (int i=0; ; i++) {
        int msgType = fifoCom->readJsonLine(data);
        if(msgType == -1){
            return;
        }
        switch (msgType) {
        case FIFO_RETURN_VAL:
            if(! qVariantTo<int>(data, &cmdInfo->returnVal)){
                logWarning <<  qtr("bad return value '%1' received").arg(data);
            }
            break;        
        case FIFO_UNMARK_PID:
            pid_t pid;
            if(! qVariantTo<int>(data, &pid)){
                logWarning <<  qtr("bad pid '%1' received").arg(data);
            } else {
                try {
                    shournalk->removePid(pid);
                } catch (const ExcShournalk& ex) {
                    logWarning << ex.what();
                }
            }
            break;
        default:
            logWarning << "Invalid fifo-message received:"
                       << msgType << "with data" << data;
            break;
        }
    }

}

static int do_polling(ShournalK_ptr& shournalk,
                      struct shournalk_run_result* run_result,
                      const QByteArray& fifopath,
                      CommandInfo* cmdInfo){
    int fifo = -1;
    auto finallyCloseFifo = finally([&fifo] {
        if(fifo != -1) close(fifo);
    });
    // Protect client from deadlock: first delete, then
    // close ("finally" reverses call order).
    auto finallydelFifo = finally([&fifopath] {
        if(!fifopath.isEmpty()){
            // fail silently, as the shell integration
            // might be faster than us
            remove(fifopath);
        }
    });

    shared_ptr<FifoCom> fifoCom;
    QVector<pollfd> fds;

    shournalk->preparePollOnce();
    pollfd shournalkfd;
    shournalkfd.fd = shournalk->kgrp()->pipe_readend;
    shournalkfd.events = POLLIN;
    fds.push_back(shournalkfd);

    if(! fifopath.isEmpty()){
        pollfd fd;
        // open RDWR, to correctly get EAGAIN in case of no (other) writer.
        fifo = os::open(fifopath, os::OPEN_RDWR | os::OPEN_NONBLOCK | os::OPEN_EXCL);
        fd.fd = fifo;
        fd.events = POLLIN;
        fds.push_back(fd);

        fifoCom = make_shared<FifoCom>(fifo);
    }

    while (1) {
        int poll_num = poll(fds.data(), nfds_t(fds.size()), -1);
        if (poll_num == -1) {
            if (errno == EINTR){     // Interrupted by a signal
                continue;            // Restart poll()
            }
            logCritical << "Error during poll: " << translation::strerror_l();
            return errno;
        }

        // 0 only on timeout, which is infinite
        assert(poll_num != 0);

        if (fds[0].revents & POLLIN) {
            auto read_count = os::read(shournalk->kgrp()->pipe_readend, run_result,
                                       sizeof(struct shournalk_run_result));
            if(read_count != sizeof(struct shournalk_run_result)){
                logCritical << qtr("Received bad run-result from kernel backend: "
                                   "expected %1 bytes but received %2.")
                               .arg(sizeof(struct shournalk_run_result)).arg(read_count);
                return EPIPE;
            }
            return 0;
        }
        assert(fds.size() > 1);

        if(fds[1].revents & POLLIN){
            handleFifoEvent(fifoCom, cmdInfo, shournalk);
        } else {
            // can never happen, because we opened the
            // fifo RDWR, so we get no events if a writer closes
            // (which is not the case if opened RDONLY)
            assert(false);
        }
    }
}



QByteArray Filewatcher_shournalk::fifopathForPid(pid_t pid)
{
    QByteArray fifopath = pathJoinFilename(QDir::tempPath().toUtf8(),
                                "shournal-run-fifo-" + QByteArray::number(pid));
    return fifopath;
}

Filewatcher_shournalk::Filewatcher_shournalk()
{

}


void Filewatcher_shournalk::setArgv(char **argv, int argc)
{
    m_commandArgv = argv;
    m_commandArgc = argc;
}

void Filewatcher_shournalk::setPid(const pid_t &pid)
{
    m_pid = pid;
}


void Filewatcher_shournalk::setCommandFilename(char *commandFilename)
{
    m_commandFilename = commandFilename;
}

void Filewatcher_shournalk::setStoreToDatabase(bool storeToDatabase)
{
    m_storeToDatabase = storeToDatabase;
}

void Filewatcher_shournalk::setShellSessionUUID(const QByteArray &shellSessionUUID)
{
    m_shellSessionUUID = shellSessionUUID;
}

void Filewatcher_shournalk::setForkIntoBackground(bool value)
{
    m_forkIntoBackground = value;
}

void Filewatcher_shournalk::setCmdString(const QString &cmdString)
{
    m_cmdString = cmdString;
}

void Filewatcher_shournalk::setFifoname(const QByteArray &fifoname)
{
    m_fifoname = fifoname;
}

void Filewatcher_shournalk::setPrintSummary(bool printSummary)
{
    m_printSummary = printSummary;
}


CommandInfo Filewatcher_shournalk::runExec(ShournalK_ptr &shournalk,
                                           CEfd& toplvlEfd)
{
    CommandInfo cmdInfo =  CommandInfo::fromLocalEnv();
    cmdInfo.sessionInfo.uuid = m_shellSessionUUID;

    if(m_commandFilename != nullptr){
        cmdInfo.text += QString(m_commandFilename) + " ";
        // TODO: rather store cmdInfo.text only from &m_commandArgv[1] in case
        // of m_commandFilename != null ?
    }
    cmdInfo.text += argvToQStr(m_commandArgc, m_commandArgv);

    CEfd cefd;
    Subprocess proc;
    proc.setWaitForSetup(false);
    proc.setCallbackAsChild([&cefd]{
        // Block until parent process did the setup
        cefd.recvMsg();
    });

    const char* cmdFilename = (m_commandFilename == nullptr) ? m_commandArgv[0]
            : m_commandFilename;
    cmdInfo.startTime = QDateTime::currentDateTime();
    proc.call(cmdFilename, m_commandArgv);

    uint64_t markRet;
    try {
        shournalk->doMark(proc.lastPid());
        markRet = CEfd::MSG_OK;
    } catch (const ExcShournalk& ex) {
        logWarning << ex.descrip();
        markRet = CEfd::MSG_FAIL;
    }
    toplvlEfd.sendMsg(markRet);

    cefd.sendMsg(markRet);
    cefd.teardown();

    try {
        cmdInfo.returnVal = proc.waitFinish();
    } catch (const os::ExcProcessExitNotNormal& ex) {
        // return typical shell cpp_exit code
        cmdInfo.returnVal = 128 + ex.status();
    }
    // do not set endTime here, but after poll for kgrp, so
    // all background-processes finished
    if(markRet != CEfd::MSG_OK){
        cpp_exit(cmdInfo.returnVal);
    }

    return cmdInfo;
}

CommandInfo Filewatcher_shournalk::runMarkPid(ShournalK_ptr &shournalk, CEfd &toplvlEfd)
{
    assert(! m_cmdString.isEmpty());

    CommandInfo cmdInfo =  CommandInfo::fromLocalEnv();
    cmdInfo.sessionInfo.uuid = m_shellSessionUUID;
    // Start-time will likely be overwritten later
    cmdInfo.text = m_cmdString;
    cmdInfo.startTime = QDateTime::currentDateTime();

    try {
        shournalk->doMark(m_pid);
        toplvlEfd.sendMsg(CEfd::MSG_OK);
    } catch (const ExcShournalk& ex) {
        logWarning << ex.descrip();
        toplvlEfd.sendMsg(CEfd::MSG_FAIL);
        cpp_exit(1);
    }
    return cmdInfo;
}


void Filewatcher_shournalk::run()
{
    auto shournalk = make_shared<ShournalkControl>();

    CommandInfo cmdInfo;

    CEfd toplvlEfd;
    if(m_forkIntoBackground){
        // parent exits, child continues in new sid.
        // We wait for child to finish setup.
        if(os::fork() != 0){
            logDebug << "forking into background";
            auto ret = (toplvlEfd.recvMsg() == CEfd::MSG_OK) ? 0 : 1;
            exit(ret);
        }
        // child
        os::setsid();
    }

    if(m_commandArgc != 0){
        cmdInfo = runExec(shournalk, toplvlEfd);
    } else {
        cmdInfo = runMarkPid(shournalk, toplvlEfd);
        os::mkfifo(m_fifoname, 0600);
    }

    // Everything is ready and cmdInfo.workingDirectory is
    // also setup correctly. Try to act at least a bit like
    // a daemon by chdir("/"); e.g. to not block an unmount.
    // Note that in case we were launched from within the
    // shell integration the only open files should be
    // our own logfile and the eventlog-file, both
    // at locations which are usually never unmounted.
    os::chdir("/");

    struct shournalk_run_result krun_result;
    auto poll_result = do_polling(shournalk, &krun_result,
                                  m_fifoname, &cmdInfo);
    cmdInfo.endTime = QDateTime::currentDateTime();

    if(poll_result != 0){
        // should never happen. Return failure regardless
        // of launched command exit status
        cpp_exit(2);
    }
    if(krun_result.error_nb != 0){
        // may rarely happen if target file got lost during
        // event processing (stored on NFS?)
        QString msg;
        switch (krun_result.error_nb) {
        case EIO:
            msg = qtr("%1. Maybe the target"
                      "file resided on a NFS storage, which became unavailable?")
                    .arg(translation::strerror_l(EIO));
            break;
        default:
            msg = translation::strerror_l(krun_result.error_nb);
            break;
        }

        logWarning << qtr("Error %1 during file event processing "
                          "(in kernel mode, most likely non-fatal): %2")
                      .arg(krun_result.error_nb).arg(msg);
        // since it is nonfatal return cmd exit code

    }
    if(krun_result.lost_event_count != 0){
        // TODO: insert cmd-id here.
        logInfo << qtr("%1 events where lost").arg(krun_result.lost_event_count);
    }

    if(m_printSummary){
        shournal_run_common::print_summary(
                    krun_result.w_event_count, krun_result.r_event_count,
                    krun_result.lost_event_count,
                    krun_result.stored_event_count,
                    os::fstat(fileno(shournalk->tmpFileTarget())).st_size);
    }

    if(m_storeToDatabase){
        // os::lseek(fileno_unlocked(tmpFileTarget), 0, SEEK_SET);
        stdiocpp::fseek(shournalk->tmpFileTarget(), 0, SEEK_SET);
        FileEvents fileEvents;
        fileEvents.setFile(shournalk->tmpFileTarget());
        // Do not disturb other processes while we flush events to database
        os::setpriority(PRIO_PROCESS, 0, PRIO_DATABASE_FLUSH);
        try {
            cmdInfo.idInDb = db_controller::addCommand(cmdInfo);
            db_controller::addFileEvents(cmdInfo, fileEvents);
        } catch (std::exception& e) {
            // May happen, e.g. if we run out of disk space...
            logCritical << qtr("Failed to store (some) file-events to disk: %1").arg(e.what());
        }
    }
    shournalk.reset();

    cpp_exit(cmdInfo.returnVal);
}


