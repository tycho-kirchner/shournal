/*
 * To allow for leaving an observed mount namespace, which is e.g. helpful,
 * if another *interactive and observed* shell is launched within an observed
 * mount-namespace, the first time shournal starts (for a gien user)
 * a seperate process is started,
 * whose only task is to wait forever (until signaled). The process creates
 * a PID-file at the temporary dir, belongs to the real user and has the msenter group.
 * */

#include <fcntl.h>

#include <QStandardPaths>
#include <QFileInfo>
#include <QLockFile>

#include "orig_mountspace_process.h"
#include "os.h"
#include "app.h"
#include "qoutstream.h"
#include "logger.h"
#include "msenter.h"
#include "cleanupresource.h"
#include "osutil.h"
#include "fdentries.h"

using osutil::closeVerbose;

namespace  {

const char* LOCK = "_LOCK";

QString userPidPath(){
    return pathJoinFilename(QStandardPaths::writableLocation(QStandardPaths::TempLocation),
            QString(app::SHOURNAL) + "-orig-mountnamespace-"
            + os::getUserName<QString>()) ;

}

/// The grandchild sets up a never ending process with the real userid
/// and the msenter-groupid
[[noreturn]]
void setupIfNotExistAsChild(const QString& pidPath) {

    auto* allowedGroupInfo = getgrnam(app::MSENTER_ONLY_GROUP);
    if(allowedGroupInfo == nullptr){
        logCritical << qtr("group %1 does not exist on your "
                           "system but is required to setup the original "
                           "mountnamespace-process. Please add it.").arg(app::MSENTER_ONLY_GROUP);
        exit(1);
    }

    os::setgid(allowedGroupInfo->gr_gid);
    os::setuid(os::getuid());

    QLockFile lockFile(pidPath + LOCK);
    if(! lockFile.lock()){
         logCritical << qtr("failed to obtain lock on %1").arg(pidPath + LOCK);
    }
    if(QFileInfo::exists(pidPath)){
        // file was created meanwhile
        exit(0);
    }
    logDebug << "creating new pid-file for the original mount-namespace";

    QFile pidFile(pidPath);
    {
        if(! pidFile.open(QFile::OpenModeFlag::WriteOnly)){
            // should never happen
            logCritical << "Failed to open pidfile" << pidPath;
            exit(1);
        }
        QTextStream stream(&pidFile);
        stream << os::getpid();
    }
    pidFile.close();
    lockFile.unlock();

    // we are a daemon. Good practice: enter /, close all fds
    os::chdir("/");
    logger::disableLogToFile();
    for(const int fd : osutil::FdEntries()){
        try {
            os::close(fd);
        } catch (const os::ExcOs& e) {
            QIErr() << e.what();
        }
    }

    // wait for typical signals to exit
    osutil::waitForSignals();
    pidFile.remove();
    exit(0);
}

[[noreturn]]
void execAsRealsUser(const char* filename, char *commandArgv[], char **envp){
    os::setuid(os::getuid());
    os::exec(filename, commandArgv, envp);
    // never get here
}

} // namespace

/// Create a grandchild-process, which creates a pid-file (in a race-free way).
void orig_mountspace_process::setupIfNotExist()
{
    const QString pidPath = userPidPath();
    if(QFileInfo::exists(pidPath)){
        return;
    }
    auto pid = os::fork();
    if(pid != 0){
        // parent returns
        return;
    }
    // Prevent receiving signals for the process-group
    os::setsid();
    setupIfNotExistAsChild(pidPath);
    // never get here
}


/// Enter a 'original' mount-namespace which was created before (setupIfNotExist)
/// and excute a command in it. If there is no pid-file, assume, there is no such
/// process yet and simply execve as 'real' user.
/// In case of an error (pid-file points to invalid process, etc.) remove
/// the pid-file and execve as 'real' user.
void orig_mountspace_process::msenterOrig(const char *filename,
                                          char *commandArgv[], char **envp)
{
    QFile pidFile(userPidPath());
    if(! pidFile.open(QFile::OpenModeFlag::ReadOnly)){
        // may happen, if file does not exist (we are the first process).
        // If it exists, report the error (yes, short race-condition here...)
        if(pidFile.exists()){
            logWarning << qtr("Failed to open pidfile although"
                              " it exists, not joining original namespace:") << pidFile.fileName();
        } else {
            logDebug << "pidPath does not exist, not joining original namespace";
        }
        execAsRealsUser(filename, commandArgv, envp);
    }

    QTextStream stream(&pidFile);
    try {
        auto targetPid = qVariantTo_throw<pid_t>(stream.readLine());
        int targetprocDirFd = os::open("/proc/" + std::to_string(targetPid), O_DIRECTORY);
        auto closeTargetprocDirFdLater = finally([&targetprocDirFd] {
            closeVerbose(targetprocDirFd);
        });
        msenter::run(filename, commandArgv, envp, targetprocDirFd);
    } catch (const std::exception& e) {
        logWarning << qtr("Cannot join original mount-namespace: %1. "
                          "Maybe shournal-run was killed? Removing obsolete "
                          "pid-file at %2...").arg(e.what()).arg(pidFile.fileName());
        if(! pidFile.remove()){
            logWarning << qtr("Removing pid-file %2 failed.").arg(pidFile.fileName());
        }
        execAsRealsUser(filename, commandArgv, envp);
    }
}
