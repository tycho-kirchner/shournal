/* Allow joining a mount-namespace created by shournal.
 * The most elegant solution would have been to pass the mnt-fd to the observed shell
 * process and let that one call setns before each command sequence. That is
 * however not allowed. TODO: suggest the Kernel devs, to skip permission checks
 * for setns if the respective fd in /proc/$pid/ns was opened by root.
 * Instead we call setns before exec, which we can, because being a setuid-program.
 * Thus we should perform some permission checks, to only allow joining mount-namespaces created
 * by shournal.
 * The target pid's user-id must be the same as the caller, its group *must* be
 * app::MSENTER_ONLY_GROUP.
 * Reenter the working directory (checked race condition, because using the dirfd does not work).
 * */

#include "msenter.h"

#include <fcntl.h>
#include <error.h>
#include <climits>
#include <csignal>
#include <cassert>

#include <QDir>
#include "os.h"
#include "excos.h"
#include "qoutstream.h"
#include "util.h"
#include "translation.h"
#include "osutil.h"
#include "pidcontrol.h"
#include "cleanupresource.h"
#include "app.h"
#include "logger.h"
#include "fdentries.h"

using osutil::closeVerbose;

/// @overload
void msenter::run(pid_t targetPid, const char* filename, char *commandArgv[], char **envp)
{
    int targetprocDirFd = os::open("/proc/" + std::to_string(targetPid), O_DIRECTORY);
    auto closeFdLater = finally([&targetprocDirFd] {  closeVerbose(targetprocDirFd);  });
    run(filename, commandArgv, envp, targetprocDirFd);
}

/// @param targetprocDirFd: an open directory descriptor of the target process.
/// The caller is responsible, for closing it, if desired
void msenter::run(const char* filename, char *commandArgv[], char **envp, int targetprocDirFd)
{
    struct stat targetPidSt =  os::fstat(targetprocDirFd);

    auto* allowedGroupInfo = getgrnam(app::MSENTER_ONLY_GROUP);
    if(allowedGroupInfo == nullptr){
        logCritical << qtr("group %1 does not exist on your "
                       "system but is required. Please add it.").arg(app::MSENTER_ONLY_GROUP);
        exit(1);
    }

    auto realUid = os::getuid();
    if (realUid != targetPidSt.st_uid ) {
        logCritical << qtr("Target process belongs to a "
                       "different user.");
        exit(1);
    }

    if( allowedGroupInfo->gr_gid != targetPidSt.st_gid){
        logCritical << qtr("The group of the target process is not '%1'")
                       .arg(app::MSENTER_ONLY_GROUP);
        exit(1);
    }
    bool setnsSuccess = false;
    bool openOrigCwdSuccess = false;
    try {
        // Remember the old working dir (setns changes it). Note that it is
        // not possible to fchdir back to oldWdFd, because doing
        // so leads also back to the original mountspace...
        // Opening the dir as *real* user is essential on NFS -> permissions.
        os::seteuid(realUid);
        const int oldWdFd = os::open(".", O_DIRECTORY);
        openOrigCwdSuccess = true;

        os::seteuid(os::getsuid());

        int mntFd = os::openat(targetprocDirFd , "ns/mnt" , O_RDONLY);
        os::setns(mntFd, CLONE_NEWNS);
        setnsSuccess = true;
        os::close(mntFd);

        // Drop root privileges, irrevocable.
        os::setuid(realUid);

        try {
            const int newWdFd = osutil::reopenFdByPath( oldWdFd, O_DIRECTORY, true, false);
            auto closeNewWdLater = finally([&newWdFd] {  closeVerbose(newWdFd);  });
            // reenter the working directory in new mount namespace which is surely the same
            // filesystem entry
            os::fchdir(newWdFd);
        } catch (const os::ExcOs& e) {
            // Should almost never happen. In that case
            // enter the working dir in the original mount-namespace.
            // File-events, referring to relative paths might then be lost.
            logWarning << qtr("Failed to enter working directory within the new mount-namespace. "
                              "Entering the original one instead. Some file-events might "
                              "be lost. Reason: %1").arg(e.what());
            os::fchdir(oldWdFd);
        }
        os::close(oldWdFd);

    } catch (const os::ExcOs & ex) {
        logCritical << ex.what();        
        if(! openOrigCwdSuccess){
            QString moreInfo;
            if( ex.errorNumber() == ESTALE){
                moreInfo = qtr("Most probably it was deleted. ");
            }
            logCritical << qtr("Failed to open the working directory. %1").arg(moreInfo);
        }
        if(! setnsSuccess){
            // most probably setns failed, because we are not suid. Since this program is execve'd
            // itself, above error message might not be visible for the user. So be gentle and
            // do not exit here.
            logCritical << qtr("Entering the mount-namespace at %1 failed, file events are not "
                               "captured...")
                           .arg(osutil::findPathOfFd<QByteArray>(targetprocDirFd).data());

        }
        os::setuid(os::getuid());
    }

    execvpe(filename, commandArgv, envp);
    int err = errno;
    // Only get here on error.
    // Failed to launch the executable - print error and mimic shell-return-codes.
    translation::init();
    QErr() << filename << ": " << translation::strerror_l(err) << "\n";
    // In bash and zsh at least the following special cases exist:
    switch (err) {
    case EACCES: exit(126);
    case ENOENT: exit(127);
    // TODO: 128 + errno?
    default: exit(1);
    }

}
