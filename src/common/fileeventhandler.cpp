
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
    m_writeEvents.reserve(1000);
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
/// If both set (not-empty) only one has to match,
/// if both unset, accept all,
/// else only take the set one into account.
bool FileEventHandler::readFileTypeMatches(const Settings::ScriptFileSettings &scriptCfg,
                                           int fd, const std::string& fpath)
{
    if(! scriptCfg.includeExtensions.empty() && ! scriptCfg.includeMimetypes.empty()){
        // both not empty, consider both (OR'd)
        return fileExtensionMatches(scriptCfg.includeExtensions, fpath) ||
               mimeTypeMatches(fd, scriptCfg.includeMimetypes);
    }
    if(scriptCfg.includeExtensions.empty() && scriptCfg.includeMimetypes.empty()){
        return true;
    }
    // one is empty, the other not
    if(! scriptCfg.includeExtensions.empty()){
        return fileExtensionMatches(scriptCfg.includeExtensions, fpath);
    }
    assert(! scriptCfg.includeMimetypes.empty());
    return mimeTypeMatches(fd, scriptCfg.includeMimetypes);
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

/// @param enableReadActions: if false, do not read from fd, regardless of settings
/// @throws ExcOs, CXXHashError
void FileEventHandler::handleCloseWrite(int fd)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    const auto filepath = readLinkOfFd(fd);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        logDebug << "closedwrite-event ignored (file deleted):"
                 << filepath;
        return;
    }

    if(! userHasWritePermission(st)){
        logDebug << "closedwrite-event ignored (no write permission):"
                 << filepath;
        return;
    }
    auto & sets = Settings::instance();

    auto & wsets = sets.writeFileSettings();
    if(wsets.excludeHidden && pathIsHidden(filepath) &&
            ! wsets.includePathsHidden.isSubPath(filepath, true)){
        logDebug << "closedwrite-event ignored (hidden file):"
                 << filepath;
        return;
    }

    if(! wsets.includePaths.isSubPath(filepath, true) ){
        logDebug << "closedwrite-event ignored (no subpath of include_dirs): "
                 << filepath;
        return;
    }
    if(wsets.excludePaths.isSubPath(filepath, true) ){
        logDebug << "closedwrite-event ignored (subpath of exclude_dirs): "
                 << filepath;
        return;
    }

    auto & writeEvent = m_writeEvents[DevInodePair(st.st_dev, st.st_ino)] ;
    writeEvent.fullPath = filepath;
    writeEvent.mtime = st.st_mtime;
    writeEvent.size = st.st_size;

    if(sets.hashSettings().hashEnable){
        writeEvent.hash =  m_hashControl.genPartlyHash(fd, st.st_size,
                                                       sets.hashSettings().hashMeta);
    }

    logDebug << "closedwrite-event recorded: "
             << writeEvent.fullPath;

    // maybe_todo: reimplement that, if desired (?).
    // if(m_pArgparse->getCommandline()){
    //     info.cmdline = pidcontrol::findCmdlineOfPID(pid); // PID from fanotify event data...
    // }
}

bool FileEventHandler::generalReadSettingsSayLogIt(const bool userHasWritePerm,
                                                   const std::string& filepath)
{
    const auto& cfg = Settings::instance().readFileSettins();
    if(! cfg.enable){
        return false;
    }
    if(cfg.onlyWritable && ! userHasWritePerm){
        logDebug << "general read event ignored: no write permission:"
                 << filepath;
        return false;
    }
    if(cfg.excludeHidden && pathIsHidden(filepath) &&
            ! cfg.includePathsHidden.isSubPath(filepath, true)){
        logDebug << "general read event ignored: hidden file:"
                 << filepath;
        return false;
    }

    if( ! cfg.includePaths.isSubPath(filepath, true)){
        logDebug << "general read event ignored: not a subpath of any included path:"
                 << filepath;
        return false;
    }
    if( cfg.excludePaths.isSubPath(filepath, true)){
        logDebug << "general read event ignored: is a subpath of an excluded path:"
                 << filepath;
        return false;
    }
    return true;
}

bool
FileEventHandler::scriptReadSettingsSayLogIt(bool userHasWritePerm,
                                                  const std::string &fpath,
                                                  const os::stat_t &st,
                                                  int fd)
{
    const auto& scriptCfg = Settings::instance().readEventScriptSettings();
    if(! scriptCfg.enable){
        return false;
    }
    // repeat check here: fanotify-read-events are only unregistered, if
    // general read events are disabled...
    if(countOfCollectedReadFiles() >= scriptCfg.maxCountOfFiles){
        logDebug << "possible script-event ignored: already collected enough files:"
                 << fpath;
        return false;
    }

    if(scriptCfg.onlyWritable && ! userHasWritePerm){
        logDebug << "possible script-event ignored: no write permission:"
                 << fpath;
        return false;
    }

    if(st.st_size > scriptCfg.maxFileSize){
        logDebug << "possible script-event ignored: file too big:"
                 << fpath;
        return false;
    }

    if(scriptCfg.excludeHidden && pathIsHidden(fpath) &&
            ! scriptCfg.includePathsHidden.isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: hidden file:"
                 << fpath;
        return false;
    }

    if( ! scriptCfg.includePaths.isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: file"
                 << fpath << "is not a subpath of any included path";
        return false;
    }
    if( scriptCfg.excludePaths.isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: file"
                 << fpath << "is a subpath of an excluded path";
        return false;
    }

    if(! readFileTypeMatches(scriptCfg, fd, fpath)){
        logDebug << "script-event ignored: neither file-extension nor mime-type "
                    "matches for " << fpath;
        return false;
    }
    return true;
}

bool FileEventHandler::pathIsHidden(const std::string &fullPath)
{
    return fullPath.find("/.") != std::string::npos;
}


/// @param enableReadActions: if false, do not read from fd, regardless of settings
void FileEventHandler::handleCloseRead(int fd)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    const auto fpath = readLinkOfFd(fd);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        logDebug << "read-event ignored (file deleted): "
                 << fpath;
        return;
    }

    if(! userHasReadPermission(st)){
        logDebug << "read-event ignored (read not allowed): "
                 << fpath;
        return;
    }
    const bool userHasWritePerm = userHasWritePermission(st);
    const bool logGeneralReadEvent = generalReadSettingsSayLogIt(userHasWritePerm,
                                                                 fpath);
    bool logScriptEvent = scriptReadSettingsSayLogIt(userHasWritePerm, fpath,
                                                     st, fd);
    if(! logGeneralReadEvent && ! logScriptEvent){
        return;
    }
    logDebug << "closedread-event recorded (collect script:" << logScriptEvent << ")"
             << fpath;

    auto & readEvent = m_readEvents[DevInodePair(st.st_dev, st.st_ino)] ;
    readEvent.fullPath = fpath;
    readEvent.mtime = st.st_mtime;
    readEvent.size = st.st_size;
    readEvent.mode = st.st_mode;

    assert(os::ltell(fd) == 0);
    auto & sets = Settings::instance();
    if(sets.hashSettings().hashEnable){
        readEvent.hash =  m_hashControl.genPartlyHash(fd, st.st_size,
                                                       sets.hashSettings().hashMeta);
    }
    if(logScriptEvent){
        assert(os::ltell(fd) == 0);
        readEvent.bytes = osutil::readWholeFile(fd, static_cast<int>(st.st_size) + 1024);
        // maybe_todo: To be really correct one would also need to check again,
        // if bytes.size > readCfg.maxFileSize. In practice this should not be too relevant...
        os::lseek(fd, 0, SEEK_SET);
        m_sizeOfCachedReadFiles += readEvent.bytes.size();
    } else {
        // should happen seldom: inode was reused, so override bytes just in case
        readEvent.bytes = {};
    }
}






