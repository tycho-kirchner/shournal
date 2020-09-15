
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
#include "app.h"
#include "strlight_util.h"



/// meant to be called as real user within the original mount namespace
FileEventHandler::FileEventHandler() :
    m_filecacheDir(QDir::tempPath() + '/' + app::SHOURNAL_RUN + "-cache-" + QString::number(os::getpid())),
    m_writeEvents(m_filecacheDir.path().toUtf8()),
    m_readEvents(m_filecacheDir.path().toUtf8()),
    m_uid(os::getuid()),
    m_ourProcFdDirDescriptor(os::open("/proc/self/fd", O_DIRECTORY)),
    m_pathbuf(PATH_MAX + 1, '\0'),
    m_fdStringBuf(snprintf( nullptr, 0, "%d", std::numeric_limits<int>::max()) + 1, '\0'),
    r_wCfg(Settings::instance().writeFileSettings()),
    r_rCfg(Settings::instance().readFileSettings()),
    r_scriptCfg(Settings::instance().readEventScriptSettings()),
    r_hashCfg(Settings::instance().hashSettings())
{
    this->fillAllowedGroups();
    if(r_hashCfg.hashEnable){
        // This is typically larger than maxCountOfReads*chunkSize
        // but does not have to be.
        m_hashControl.getXXHash().reserveBufSize(1024*512);
    }
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
                                           int fd, const StrLight& fpath)
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

void FileEventHandler::readLinkOfFd(int fd, StrLight &output)
{
    assert(m_ourProcFdDirDescriptor != -1);
    // uitoa, safe in this context (but not in general),
    // is a lot faster, so do not use snprintf here.
    // snprintf( &m_fdStringBuf[0], m_fdStringBuf.size() (+1?), "%d", fd);
    util_performance::uitoa(fd, m_fdStringBuf.data());
    ssize_t path_len = ::readlinkat(m_ourProcFdDirDescriptor,
                                    m_fdStringBuf.data(),
                                    output.data(), output.capacity());
    if (path_len == -1 ){
        throw os::ExcReadLink("readlinkat failed for fd " + std::to_string(fd));
    }
    output.resize(StrLight::size_type(path_len));
}

bool FileEventHandler::fileExtensionMatches(const Settings::StrLightSet &validExtensions,
                                            const StrLight& fullPath)
{
    strlight_util::findFileExtension_raw(fullPath, m_extensionBuf);
    if(m_extensionBuf.empty()){
        return false;
    }
    return validExtensions.find(m_extensionBuf) != validExtensions.end();
}

bool FileEventHandler::mimeTypeMatches(int fd, const Settings::MimeSet &validMimetypes)
{
    QFdDummyDevice f(fd);
    const auto mimetype = m_mimedb.mimeTypeForData(&f).name();
    os::lseek(fd, 0, SEEK_SET);
    return validMimetypes.find(mimetype) != validMimetypes.end();
}


QString FileEventHandler::getTmpDirPath() const
{
    return m_filecacheDir.path();
}


void FileEventHandler::clearEvents()
{
    m_writeEvents.clear();
    m_readEvents.clear();
}

FileWriteEvents &FileEventHandler::writeEvents()
{
    return m_writeEvents;
}

FileReadEvents &FileEventHandler::readEvents()
{
    return m_readEvents;
}


/// @param enableReadActions: if false, do not read from fd, regardless of settings
/// @throws ExcOs, CXXHashError
void FileEventHandler::handleCloseWrite(int fd)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    readLinkOfFd(fd, m_pathbuf);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        logDebug << "closedwrite-event ignored (file deleted):"
                 << m_pathbuf;
        return;
    }

    if(! userHasWritePermission(st)){
        logDebug << "closedwrite-event ignored (no write permission):"
                 << m_pathbuf;
        return;
    }

    if(! r_wCfg.includePaths->isSubPath(m_pathbuf, true) ){
        logDebug << "closedwrite-event ignored (no subpath of include_dirs): "
                 << m_pathbuf;
        return;
    }
    if(r_wCfg.excludePaths->isSubPath(m_pathbuf, true) ){
        logDebug << "closedwrite-event ignored (subpath of exclude_dirs): "
                 << m_pathbuf;
        return;
    }

    if(r_wCfg.excludeHidden && pathIsHidden(m_pathbuf) &&
            ! r_wCfg.includePathsHidden->isSubPath(m_pathbuf, true)){
        logDebug << "closedwrite-event ignored (hidden file):"
                 << m_pathbuf;
        return;
    }

    HashValue hash;
    if(r_hashCfg.hashEnable){
        hash =  m_hashControl.genPartlyHash(fd, st.st_size,
                                                       r_hashCfg.hashMeta);
    }
    m_writeEvents.write(m_pathbuf, st, hash);

    logDebug << "closedwrite-event recorded: "
             << m_pathbuf;

    // maybe_todo: reimplement that, if desired (?).
    // if(m_pArgparse->getCommandline()){
    //     info.cmdline = pidcontrol::findCmdlineOfPID(pid); // PID from fanotify event data...
    // }
}

bool FileEventHandler::generalReadSettingsSayLogIt(const bool userHasWritePerm,
                                                   const StrLight& filepath)
{
    if(! r_rCfg.enable){
        return false;
    }
    if(r_rCfg.onlyWritable && ! userHasWritePerm){
        logDebug << "general read event ignored: no write permission:"
                 << filepath;
        return false;
    }

    if( ! r_rCfg.includePaths->isSubPath(filepath, true)){
        logDebug << "general read event ignored: not a subpath of any included path:"
                 << filepath;
        return false;
    }
    if( r_rCfg.excludePaths->isSubPath(filepath, true)){
        logDebug << "general read event ignored: is a subpath of an excluded path:"
                 << filepath;
        return false;
    }

    if(r_rCfg.excludeHidden && pathIsHidden(filepath) &&
            ! r_rCfg.includePathsHidden->isSubPath(filepath, true)){
        logDebug << "general read event ignored: hidden file:"
                 << filepath;
        return false;
    }

    return true;
}

bool
FileEventHandler::scriptReadSettingsSayLogIt(bool userHasWritePerm,
                                                  const StrLight &fpath,
                                                  const os::stat_t &st,
                                                  int fd)
{
    if(! r_scriptCfg.enable){
        return false;
    }
    // repeat check here: fanotify-read-events are only unregistered, if
    // general read events are disabled...
    if(m_readEvents.getStoredFilesCounter() >= r_scriptCfg.maxCountOfFiles){
        logDebug << "possible script-event ignored: already collected enough files:"
                 << fpath;
        return false;
    }

    if(r_scriptCfg.onlyWritable && ! userHasWritePerm){
        logDebug << "possible script-event ignored: no write permission:"
                 << fpath;
        return false;
    }

    if(st.st_size > r_scriptCfg.maxFileSize){
        logDebug << "possible script-event ignored: file too big:"
                 << fpath;
        return false;
    }

    if( ! r_scriptCfg.includePaths->isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: file"
                 << fpath << "is not a subpath of any included path";
        return false;
    }
    if( r_scriptCfg.excludePaths->isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: file"
                 << fpath << "is a subpath of an excluded path";
        return false;
    }

    if(r_scriptCfg.excludeHidden && pathIsHidden(fpath) &&
            ! r_scriptCfg.includePathsHidden->isSubPath(fpath, true)){
        logDebug << "possible script-event ignored: hidden file:"
                 << fpath;
        return false;
    }

    if(! readFileTypeMatches(r_scriptCfg, fd, fpath)){
        logDebug << "script-event ignored: neither file-extension nor mime-type "
                    "matches for " << fpath;
        return false;
    }
    return true;
}

bool FileEventHandler::pathIsHidden(const StrLight &fullPath)
{
    return fullPath.find("/.") != StrLight::npos;
}


/// @param enableReadActions: if false, do not read from fd, regardless of settings
void FileEventHandler::handleCloseRead(int fd)
{
    // first lookup the path, then stat, so no filename contains a trailing '(deleted)'
    readLinkOfFd(fd, m_pathbuf);
    const auto st = os::fstat(fd);
    if(st.st_nlink == 0){
        // always ignore deleted files
        logDebug << "read-event ignored (file deleted): "
                 << m_pathbuf;
        return;
    }

    if(! userHasReadPermission(st)){
        logDebug << "read-event ignored (read not allowed): "
                 << m_pathbuf;
        return;
    }
    const bool userHasWritePerm = userHasWritePermission(st);
    const bool logGeneralReadEvent = generalReadSettingsSayLogIt(userHasWritePerm,
                                                                 m_pathbuf);
    bool logScriptEvent = scriptReadSettingsSayLogIt(userHasWritePerm, m_pathbuf,
                                                     st, fd);
    if(! logGeneralReadEvent && ! logScriptEvent){
        return;
    }

    HashValue hash;
    if(r_hashCfg.hashEnable){
        assert(os::ltell(fd) == 0);
        hash =  m_hashControl.genPartlyHash(fd, st.st_size,
                                                       r_hashCfg.hashMeta);
    }
    if(logScriptEvent){
        assert(os::ltell(fd) == 0);
    }

    m_readEvents.write(m_pathbuf, st, hash, fd, logScriptEvent);

    logDebug << "closedread-event recorded (collect script:" << logScriptEvent << ")"
             << m_pathbuf;
}








