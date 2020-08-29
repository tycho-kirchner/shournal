
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE // Needed to get O_LARGEFILE definition
#endif

#include <sys/fanotify.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#include <iostream>
#include <cstring>
#include <cstddef>
#include <sys/mount.h>
#include <cstring>
#include <array>


#include "fanotify_controller.h"
#include "util.h"
#include "fileeventhandler.h"
#include "excos.h"
#include "cxxhash.h"
#include "os.h"
#include "osutil.h"
#include "settings.h"
#include "logger.h"
#include "translation.h"
#include "mount_controller.h"
#include "db_connection.h"
#include "storedfiles.h"


using ExcCXXHash = CXXHash::ExcCXXHash;
using os::ExcOs;
using StringSet = Settings::StringSet;
using osutil::closeVerbose;

namespace  {

QString fanotifyEventMaskToStr(uint64_t m){
    QString action;
    if(m & FAN_MODIFY){
        action += "modified ";
    }
    if(m & FAN_CLOSE_WRITE){
        action += "closed_write ";
    }
    if(m & FAN_CLOSE_NOWRITE){
        action += "closed_nowrite";
    }
    if(m & FAN_OPEN){
        action += "open";
    }

    if(action.isEmpty()){
        action = "unhandled event: " + QString::number(m, 16);
    }
    return action;
}

bool fanotifyMarkWrapOnInit(int fanFd, uint64_t mask, const std::string& path_){
    if (fanotify_mark(fanFd, FAN_MARK_ADD | FAN_MARK_MOUNT,
                      mask, AT_FDCWD,
                      path_.c_str()) == -1) {

        const auto msg = qtr("fanotify_mark: failed to add path %1. "
                             "It will not be observed: %2 failed - %3(%4)")
                         .arg(path_.c_str(), fanotifyEventMaskToStr(mask),
                              translation::strerror_l()).arg(errno);
        if(Settings::instance().getMountIgnoreNoPerm() && errno == EACCES){
            logDebug << msg;
        } else {
            logWarning << msg;
        }
        return false;
    }
    logDebug << "fanotify_mark" << fanotifyEventMaskToStr(mask) << path_;
    return true;

}

/// Fill param result with parentPaths and all sub-mountpaths, that is,
/// all paths in allMountpaths, which are a sub-path of any parentPath,
/// are added.
void addPathsAndSubMountPaths(const std::shared_ptr<PathTree>& parentPaths,
                              const std::shared_ptr<PathTree>& allMountpaths,
                              StringSet& result){
    for(const auto& p : *parentPaths){
        result.insert(p);
        for(auto mountIt = allMountpaths->subpathIter(p); mountIt != allMountpaths->end(); ++mountIt){
            result.insert(*mountIt);
        }
    }
}


} // anonymous namespace




/// Initialize fanotify's filedescriptor (requires root)
/// @throws ExcOs
FanotifyController::FanotifyController(FileEventHandler &feventHandler) :
    m_feventHandler(feventHandler),
    m_overflowOccurred(false),
    m_fanFd(-1),
    m_markLimitReached(false),
    m_ReadEventsUnregistered(false)
{
    // Create the file descriptor for accessing the fanotify API
    m_fanFd = fanotify_init(FAN_CLOEXEC | FAN_NONBLOCK,
                       O_RDONLY | O_LARGEFILE | O_CLOEXEC | O_NOATIME);
    if (m_fanFd == -1) {
        throw ExcOs("fanotify_init failed:");
    }
}

FanotifyController::~FanotifyController(){
    try {
        os::close(m_fanFd);
    } catch (const std::exception& e) {
        logCritical << __func__ << e.what();
    }
}



bool FanotifyController::overflowOccurred() const
{
    return m_overflowOccurred;
}

int FanotifyController::fanFd() const
{
    return m_fanFd;
}

/// fanotify_mark all paths of interest, that is all paths
/// which shall be observed for write-events (file modifications) or read events.
/// We unshared the mount-namespace before, perform the
/// mark by using mount-points.
/// Also collect all mount-points, which are submounts of
/// a desired path (if / shall be observed, e.g.
/// mark filesystems under /media as well).
/// Note that on marking a path, parent directories are possibly marked
/// as well, if the mount-point lays above it.
/// ( if mountpoint is /home and the dir /home/user/foo shall be observed,
///   fanotify_mark marks /home, thus events occuring in /home and /home/user
///   are also reported [and need to be filtered out later]).
void FanotifyController::setupPaths(){
    m_ReadEventsUnregistered = false;

    auto & sets = Settings::instance();
    auto allMounts = mountController::generatelMountTree();
    StringSet allWritePaths;
    addPathsAndSubMountPaths(sets.writeFileSettings().includePaths,
                             allMounts, allWritePaths);

    StringSet allReadPaths;
    // Script files (which shall be stored) and 'normal' read files are treated differently later -
    // first mark unified paths from both categories for fanotify read-events.
    if(sets.readFileSettins().enable){
        addPathsAndSubMountPaths(sets.readFileSettins().includePaths,
                                 allMounts, allReadPaths);
    }
    if(sets.readEventScriptSettings().enable){
        addPathsAndSubMountPaths(sets.readEventScriptSettings().includePaths,
                                 allMounts, allReadPaths);
    }

    m_readMountPaths.reserve(allReadPaths.size());

    uint64_t writeMask = FAN_CLOSE_WRITE;
    if(! sets.writeFileSettings().onlyClosedWrite){
        writeMask |= FAN_MODIFY;
    }
    uint64_t readMask = FAN_CLOSE_NOWRITE;
    uint64_t readWriteMask = readMask | writeMask;

    for(const auto & p : allWritePaths){
        auto pathInReadIt = allReadPaths.find(p);
        uint64_t m = writeMask;
        if(pathInReadIt != allReadPaths.end()){
            // path interesting for both, read and write
            m = readWriteMask;
            allReadPaths.erase(pathInReadIt);
        }

        if(fanotifyMarkWrapOnInit(m_fanFd, m, p) && m & readMask){
            // once the specified number of read files was collected,
            // the read paths shall be unregistered again. So store the paths.
            m_readMountPaths.push_back(p);
        }
    }

    // also add read paths not already marked above (along with write-paths).
    for(const auto & p : allReadPaths){
        if(fanotifyMarkWrapOnInit(m_fanFd, readMask, p)){
            m_readMountPaths.push_back(p);
        }
    }

    // ignore file events we generate ourselves
    ignoreOwnPath(db_connection::getDatabaseDir().toUtf8());
    ignoreOwnPath(StoredFiles::getReadFilesDir().toUtf8());
    ignoreOwnPath(logger::logDir().toUtf8());
}



/// Handle fanotify events.
/// For a general introduction please see man fanotify.
bool FanotifyController::handleEvents()
{
    struct fanotify_event_metadata *metadata;
    struct fanotify_event_metadata buf[8192];
    ssize_t len;

    // Loop while events can be read from fanotify file descriptor
    while(true) {
        // Read some events
        len = read(m_fanFd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            const auto preamble = qtr("read from fanotify file descriptor failed:");
            // maybe_todo: file a bug to the fanotify-devs? According to man 7 fanotify
            // there should be no permission check, when the kernel repoens the file
            // for fanotify...
            // Furthermore it is unclear whether other events in the queue after a
            // bad fd are gone as well.
            switch (errno) {
            case ENOENT: break; // a deleted file is not of interest anyway. ignore.
            case EACCES:
                logInfo << preamble
                        << qtr("EACCES most likely occurred, because a not readable "
                               "file was closed on a NFS-storage, or similar.");
                break;
            default:
                logWarning << preamble << "(" + QString::number(errno) + ") -"
                            << translation::strerror_l();
                break;
            }
            return false;
        }

        // Check if end of available data reached
        if (len <= 0) {
            return true;
        }
        logDebug << "read"
                 << static_cast<size_t>(len) / sizeof(fanotify_event_metadata) << "events";

        // Point to the first event in the buffer
        metadata = buf;

        // Loop over all events in the buffer
        while (FAN_EVENT_OK(metadata, len)) {
            // Check that run-time and compile-time structures match
            if (metadata->vers != FANOTIFY_METADATA_VERSION) {
                logCritical << qtr("Mismatch of fanotify metadata version - runtime: %1, "
                                   "compiletime: %2. "
                                   "No event-processing takes place. "
                                   "Please recompile the application against the current "
                                   "Kernel").arg(metadata->vers, FANOTIFY_METADATA_VERSION);
                // maybe_todo: unregister from all events?
                return false;
            }
            // metadata->fd contains either FAN_NOFD, indicating a
            // queue overflow, or a file descriptor (a nonnegative
            // integer).
            if (metadata->fd < 0) {
                logWarning << "fanotify: queue overflow";
                m_overflowOccurred = true;
            } else {
                handleSingleEvent(*metadata);
                closeVerbose(metadata->fd);
            }
            // Advance to next event
            metadata = FAN_EVENT_NEXT(metadata, len);
        } // while (FAN_EVENT_OK(metadata, len))
    } // while true
    // maybe_todo: add option for small delay to allow for
    // the accumulation of events within fanotify
    //usleep(1000 * 10);
}



void FanotifyController::handleSingleEvent(const struct fanotify_event_metadata& metadata){

    bool modified = metadata.mask & FAN_MODIFY;
    bool closed_write = metadata.mask & FAN_CLOSE_WRITE;
    bool closed_nowrite = metadata.mask & FAN_CLOSE_NOWRITE;
    if(metadata.mask & FAN_Q_OVERFLOW){
        logWarning << "fanotify: queue overflow";
        m_overflowOccurred = true;
    }

/* #ifndef NDEBUG
    {
        auto st = os::fstat(metadata.fd);
        std::string path;
        try {
            path = os::readlink("/proc/self/fd/" + std::to_string(metadata.fd));
        } catch (const os::ExcOs& ex) {
            logDebug << ex.what();
            path = "UNKNOWN";
        }
        auto action = fanotifyEventMaskToStr(metadata.mask);
        logDebug << action << "event-pid" << metadata.pid << path
                 << "fd:" << metadata.fd << "uid: " << st.st_uid
                 << " gid: " << st.st_gid;
    }

#endif */
    // Ignore further modify events for a filesystem-object (device/inode)
    // until it is closed.
    // Note that if only a small amount of data is written to a file,
    // it can occur that both flags are set: FAN_MODIFY and FAN_CLOSE_WRITE
    // No need to touch the ignore mask in these cases.
    if (modified && ! closed_write && ! m_markLimitReached) {
        if (fanotify_mark(m_fanFd,
                          FAN_MARK_ADD | FAN_MARK_IGNORED_MASK |
                             FAN_MARK_IGNORED_SURV_MODIFY,
                          FAN_MODIFY ,
                          metadata.fd,
                          nullptr) == 0){
            logDebug << "added to ignore mask";
        } else  {
            if(errno == ENOSPC){
                m_markLimitReached = true;
                logWarning << "fanotify mark-limit reached, "
                                 "all closed-write events are treated as "
                                 "modification event.";
            } else {
                logWarning << "fanotify_mark add to ignore mask failed for file "
                           <<os::readlink("/proc/self/fd/" + std::to_string(metadata.fd));
            }

        }
    }
    auto & sets = Settings::instance();

    if (closed_write) {
        // CLOSE_WRITE might also occur, if nothing was written.
        // However, if the file was modifed (and variable 'modified' is false)
        // removing the MODIFY from the ignore mask should succeed.
        if(modified || sets.writeFileSettings().onlyClosedWrite){
            // modifed event and closed_write event have both occurred
            // ( OR we are interested in every closed-write-event).
            handleModCloseWrite_safe(metadata);

        } else {
            // if a modification for that filesystem-object (device/inode)
            // occurred (in a previous loop), removing the modify event from
            // the ignore mask should succeed.
            // Note: the order of fanotify_mark and handleModCloseWrite_safe matters:
            // if the modification time was determined BEFORE the
            // inode was removed from the ignore mask, a modification caused by our
            // childprocess might remain unrecognised.
            int mark_res = fanotify_mark(m_fanFd,
                                         FAN_MARK_REMOVE | FAN_MARK_IGNORED_MASK
                                            | FAN_MARK_IGNORED_SURV_MODIFY,
                                         FAN_MODIFY,
                                         metadata.fd,
                                         nullptr);
            if(mark_res == 0) {
                logDebug << "removed from ignore mask";
                // there should be a free space again:
                m_markLimitReached = false;
                handleModCloseWrite_safe(metadata);

            } else if (errno == ENOENT) {
                // ENOENT is returned, if no MODIFY event in the respective
                // ignore mask exists. This can e.g. be the case, if
                // a file was opened in append mode without having written to it.
                // However, it is also possible that a file (or a hardlink to it)
                // was openend multiple times. In that case the removal of the fanotify
                // ignore mask succeeds only on the first close.
                // If our fanotify ran out of marks, also handle the event:
                if(m_markLimitReached){
                    handleModCloseWrite_safe(metadata);
                }
            } else {
                // Otherwise report the error.
                logWarning << "fanotify_mark remove from ignore mask failed for file "
                           << os::readlink("/proc/self/fd/" + std::to_string(metadata.fd));
            }
        }
    } // if (closed_write)

    if(closed_nowrite){
        handleCloseNoWrite_safe(metadata);
    }

}

/// Handle a 'read'-event.
/// If read 'script' files shall be stored, but not general read files,
/// unregister from read events, as soon as the specified number of script
/// files was collected.
void FanotifyController::handleCloseNoWrite_safe(const fanotify_event_metadata &metadata){
    if(m_ReadEventsUnregistered){
        // Do not edit: even if successfully unregistered,
        // events in the fanotify event-queue may still need to be consumed.
        return;
    }
    auto & sets = Settings::instance();
    if(! sets.readFileSettins().enable && // never unregister, if general read files are logged
         sets.readEventScriptSettings().enable &&
            m_feventHandler.countOfCollectedReadFiles() >=
            sets.readEventScriptSettings().maxCountOfFiles) {
        unregisterAllReadPaths();
        m_ReadEventsUnregistered = true;
        return;
    }

    try {
        m_feventHandler.handleCloseRead(metadata.fd);
        // The count of cached read (script-) files might have been incremented,
        // so we might be done with read events. For the sake
        // of code-shortness only check that the *next* time we consume a read event.
    } catch (const std::exception & e) {
        logCritical << e.what();
    }
}


void FanotifyController::handleModCloseWrite_safe(const fanotify_event_metadata & metadata){
    try {
        m_feventHandler.handleCloseWrite( metadata.fd );
    } catch (const std::exception & e) {
        logCritical << e.what();
    }
}

/// unregister read events for all previously marked paths.
void FanotifyController::unregisterAllReadPaths()
{
    logDebug << "enough read script-files collected. Unregistering...";
    for(const auto& p : m_readMountPaths){
        if (fanotify_mark(m_fanFd, FAN_MARK_REMOVE | FAN_MARK_MOUNT,
                          FAN_CLOSE_NOWRITE, AT_FDCWD,
                          p.c_str()) == -1) {
            logInfo << "fanotify_mark: failed to remove read-path " << p
                    <<": " << translation::strerror_l();
        }
    }

}


void FanotifyController::ignoreOwnPath(const QByteArray& p){
    if (fanotify_mark(m_fanFd,
                      FAN_MARK_ADD | FAN_MARK_IGNORED_MASK |
                      FAN_MARK_IGNORED_SURV_MODIFY | FAN_MARK_ONLYDIR,
                      FAN_ALL_EVENTS,
                      -1,
                      p.constData()) == -1){
        // should never happen...
        logCritical << "fanotify_mark: failed to ignore our own path: "
                    << strerror(errno);
    }
}


