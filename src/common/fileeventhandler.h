#pragma once

#include <sys/stat.h>
#include <string>
#include <unordered_set>
#include <QHash>
#include <QPair>
#include <QMimeDatabase>
#include <QTemporaryDir>

#include "hashcontrol.h"
#include "nullable_value.h"
#include "fileevents.h"
#include "settings.h"
#include "os.h"
#include "strlight.h"
#include "util_performance.h"


/// Collect desired file-event (read/write) metadata based on a file-descriptor.
/// The Metadata is stored within binary files at a temporary directory
/// (some read files may be stored there as a whole, based on user configuration).
/// Events are filtered beforehand, e.g. for matching user or include/exclude paths.
class FileEventHandler
{
public:
    FileEventHandler();
    ~FileEventHandler();

    void handleCloseWrite(int fd);
    void handleCloseRead(int fd);

    FileEvents& fileEvents();

    void clearEvents();

    QString getTmpDirPath() const;

public:
    Q_DISABLE_COPY(FileEventHandler)
    DISABLE_MOVE(FileEventHandler)


private:
    void fillAllowedGroups();

    bool userHasWritePermission(const struct stat& st);
    bool userHasReadPermission(const struct stat& st);
    bool readFileTypeMatches(const Settings::ScriptFileSettings& scriptCfg, int fd,
                             const StrLight &fpath);
    void readLinkOfFd(int fd, StrLight &output);

    bool fileExtensionMatches(const Settings::StrLightSet &validExtensions,
                              const StrLight &fullPath);
    bool mimeTypeMatches(int fd, const Settings::MimeSet& validMimetypes);
    bool generalReadSettingsSayLogIt(bool userHasWritePerm,
                                     const StrLight &filepath);
    bool scriptReadSettingsSayLogIt(bool userHasWritePerm,
                                    const StrLight &fpath,
                                    const os::stat_t& st,
                                    int fd);
    bool pathIsHidden(const StrLight &fullPath);

    QTemporaryDir m_filecacheDir;
    FileEvents m_fileEvents;
    HashControl m_hashControl;
    std::unordered_set<gid_t> m_groups;
    uid_t m_uid; // cached real uid
    int m_ourProcFdDirDescriptor; // holds open fd on /proc/self/fd
    QMimeDatabase m_mimedb;
    StrLight m_pathbuf;
    StrLight m_fdStringBuf;
    StrLight m_extensionBuf;

    const Settings::WriteFileSettings& r_wCfg;
    const Settings::ReadFileSettings& r_rCfg;
    const Settings::ScriptFileSettings& r_scriptCfg;
    const Settings::HashSettings& r_hashCfg;
};

