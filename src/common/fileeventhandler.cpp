
#include <cassert>
#include <string>
#include <sys/fanotify.h>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <climits>
#include <QtDebug>

#include "fileeventhandler.h"
#include "settings.h"
#include "osutil.h"
#include "excos.h"
#include "os.h"
#include "logger.h"
#include "os.h"
#include "qfddummydevice.h"
#include "qoutstream.h"



/// meant to be called as real user within the original mount namespace
FileEventHandler::FileEventHandler() :
    m_uid(os::getuid()),
    m_ourProcFdDirDescriptor(os::open("/proc/self/fd", O_DIRECTORY)),
    m_sizeOfCachedReadFiles(0)
{
    this->fillAllowedGroups();
    m_writeEvents.reserve(5000);
}

FileEventHandler::~FileEventHandler(){
    try {
        os::close(m_ourProcFdDirDescriptor);
    } catch (const os::ExcOs& e) {
        logCritical << __func__ << e.what();
    }
}

void FileEventHandler::fillAllowedGroups()
{
    auto groups = os::getgroups();
    auto egid = os::getegid();
    auto gid = os::getgid();
    for(const auto& g : groups){
        if(gid == egid || g != egid){
            // only insert the 'real' groups
            m_groups.insert(g);
        }
    }
}


/// A file-write-event is considered allowed here, if the *real user*
/// is allowed to write to a given file, be it because being the owner,
/// the file is writable by everyone or he/she is
/// part of the owning group.
/// Background is that we are interested in write events, however,
/// reporting file modifcations of a root-process should not be allowed.
bool FileEventHandler::userHasWritePermission(const struct stat &st)
{
    return (st.st_mode & S_IWUSR && st.st_uid == m_uid)  ||
            st.st_mode & S_IWOTH ||
           (st.st_mode & S_IWGRP && m_groups.find(st.st_gid) != m_groups.end());
}


/// See doc of write writeEventAllowed, replace 'write' with 'read'
bool FileEventHandler::userHasReadPermission(const struct stat &st)
{
    return (st.st_mode & S_IRUSR && st.st_uid == m_uid)  ||
            st.st_mode & S_IROTH ||
           (st.st_mode & S_IRGRP && m_groups.find(st.st_gid) != m_groups.end());

}

/// check whether to accept the file according to file extension and mime-type.
/// If both set (not-empty) only has has to match,
/// if both unset, accept all,
/// else only take the set one into account.
bool FileEventHandler::readFileTypeMatches(const Settings::ReadFileSettings &readCfg,
                                           int fd, const std::string& fpath)
{
    if(! readCfg.includeExtensions.empty() && ! readCfg.includeMimetypes.empty()){
        // both not empty, consider both (OR'd)
        return fileExtensionMatches(readCfg.includeExtensions, fpath) ||
               mimeTypeMatches(fd, readCfg.includeMimetypes);
    }
    if(readCfg.includeExtensions.empty() && readCfg.includeMimetypes.empty()){
        return true;
    }
    // one is empty, the other not
    if(! readCfg.includeExtensions.empty()){
        return fileExtensionMatches(readCfg.includeExtensions, fpath);
    }
    assert(! readCfg.includeMimetypes.empty());
    return mimeTypeMatches(fd, readCfg.includeMimetypes);
}

bool FileEventHandler::fileExtensionMatches(const Settings::StringSet &validExtensions,
                                            const std::string& fullPath)
{
    const auto& ext = getFileExtension(splitAbsPath(fullPath).second);
    return validExtensions.find(ext) != validExtensions.end();

}

bool FileEventHandler::mimeTypeMatches(int fd, const Settings::MimeSet &validMimetypes)
{
    QFdDummyDevice f(fd);
    const auto mimetype = m_mimedb.mimeTypeForData(&f).name();
    os::lseek(fd, 0, SEEK_SET);
    return validMimetypes.find(mimetype) != validMimetypes.end();
}

int FileEventHandler::sizeOfCachedReadFiles() const
{
    return m_sizeOfCachedReadFiles;
}

const FileReadEventHash &FileEventHandler::readEvents() const
{
    return m_readEvents;
}

int FileEventHandler::countOfCollectedReadFiles() const
{
    return m_readEvents.size();
}


std::string FileEventHandler::readLinkOfFd(int fd)
{
    assert(m_ourProcFdDirDescriptor != -1);
    return os::readlinkat(m_ourProcFdDirDescriptor, std::to_string(fd));
}


void FileEventHandler::clearEvents()
{
    m_writeEvents.clear();
    m_readEvents.clear();
    m_sizeOfCachedReadFiles = 0;
}


const FileWriteEventHash &FileEventHandler::writeEvents() const
{
    return m_writeEvents;
}

FileWriteEventHash &FileEventHandler::writeEvents()
{
    return m_writeEvents;
}


/// @param enableReadActions: if false, do not read from fd, regardless of settings
/// @throws ExcOs, CXXHashError
void FileEventHandler::handleCloseWrite(int fd, bool enableReadActions)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    const auto pathOfFd = readLinkOfFd(fd);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        return;
    }

    if(! userHasWritePermission(st)){
        logDebug << "closedwrite-event not recorded (no write permission): "
                 << pathOfFd;
        return;
    }
    auto & wsets = Settings::instance().writeFileSettings();
    if(! wsets.includePaths.isSubPath(pathOfFd, true) ){
        logDebug << "closedwrite-event not recorded (no subpath of include_dirs): "
                 << pathOfFd;
        return;
    }
    if(wsets.excludePaths.isSubPath(pathOfFd, true) ){
        logDebug << "closedwrite-event not recorded (subpath of exclude_dirs): "
                 << pathOfFd;
        return;
    }

    auto & writeEvent = m_writeEvents[DevInodePair(st.st_dev, st.st_ino)] ;
    writeEvent.fullPath = pathOfFd;
    writeEvent.mtime = st.st_mtime;
    writeEvent.size = st.st_size;

    if(enableReadActions && wsets.hashEnable){
        writeEvent.hash =  m_hashControl.genPartlyHash(fd, st.st_size, wsets.hashMeta);
    }


    logDebug << "closedwrite-event recorded: "
             << writeEvent.fullPath;

    // maybe_todo: reimplement that, if desired (?).
    // if(m_pArgparse->getCommandline()){
    //     info.cmdline = pidcontrol::findCmdlineOfPID(pid); // PID from fanotify event data...
    // }
}

/// @param enableReadActions: if false, do not read from fd, regardless of settings
void FileEventHandler::handleCloseNoWrite(int fd, bool enableReadActions)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    const auto fpath = readLinkOfFd(fd);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        return;
    }

    if(! userHasReadPermission(st)){
        logDebug << "close-no-write-event not recorded (not allowed): "
                 << fpath;
        return;
    }

    const auto & readCfg = Settings::instance().readEventSettings();

    if(st.st_size > readCfg.maxFileSize){
        logDebug << "close-no-write-event ignored: file too big:"
                 << fpath;
        return;
    }

    if(readCfg.onlyWritable &&
       ! userHasWritePermission(st)){
        logDebug << "close-no-write-event ignored: no write permission"
                 << fpath;
        return;
    }

    if( ! readCfg.includePaths.isSubPath(fpath, true)){
        logDebug << "close-no-write-event ignored: file"
                 << fpath << "is not a subpath of any included path";
        return;
    }
    if( readCfg.excludePaths.isSubPath(fpath, true)){
        logDebug << "close-no-write-event ignored: file"
                 << fpath << "is a subpath of an excluded path";
        return;
    }

    if(! readFileTypeMatches(readCfg, fd, fpath)){
        logDebug << "close-no-write-event ignored: neither file-extension nor mime-type "
                    "matches for path" << fpath;
        return;
    }
    logDebug << "close-no-write-event recorded"
             << fpath;

    auto & readEvent = m_readEvents[DevInodePair(st.st_dev, st.st_ino)] ;
    readEvent.fullPath = fpath;
    readEvent.mtime = st.st_mtime;
    readEvent.size = st.st_size;
    readEvent.mode = st.st_mode;

    if(enableReadActions){
        assert(os::ltell(fd) == 0);
        readEvent.bytes = osutil::readWholeFile(fd, static_cast<int>(st.st_size) + 1024);
        // maybe_todo: To be really correct one would also need to check again,
        // if bytes.size > readCfg.maxFileSize. In practice this should not be too relevant...
        os::lseek(fd, 0, SEEK_SET);
        m_sizeOfCachedReadFiles += readEvent.bytes.size();
    }

}






