#pragma once

#include <sys/stat.h>
#include <string>
#include <unordered_set>
#include <QHash>
#include <QPair>
#include <QMimeDatabase>

#include "hashcontrol.h"
#include "nullable_value.h"
#include "fileeventtypes.h"
#include "settings.h"

/// Collect all desired file-event (read/write) information  on the basis of a file-descriptor.
/// Events are stored in a map, where the key is a unique device-inode-pair.
/// Note that in case a file is deleted, the inode-number might be reused,
/// however, such a file should not be of interest though.
/// Note that in case a file is copied to another place this is again
/// a file modification event, so nothing should get lost.
/// For file read events this does not apply -> maybe_todo: use a vector, rather
/// than a hash for those?
class FileEventHandler
{
public:
    FileEventHandler();
    ~FileEventHandler();

    void handleCloseWrite(int fd, bool enableReadActions=true);
    void handleCloseNoWrite(int fd, bool enableReadActions=true);

    const FileWriteEventHash &writeEvents() const;
    FileWriteEventHash &writeEvents();

    const FileReadEventHash& readEvents() const;

    std::string readLinkOfFd(int fd);

    void clearEvents();

    int countOfCollectedReadFiles() const;

    int sizeOfCachedReadFiles() const;

public:
    FileEventHandler(const FileEventHandler&) = delete;
    void operator=(const FileEventHandler&) = delete;

private:
    void fillAllowedGroups();

    bool userHasWritePermission(const struct stat& st);
    bool userHasReadPermission(const struct stat& st);
    bool readFileTypeMatches(const Settings::ReadFileSettings& readCfg, int fd,
                             const std::string &fpath);

    bool fileExtensionMatches(const Settings::StringSet& validExtensions,
                              const std::string& fullPath);
    bool mimeTypeMatches(int fd, const Settings::MimeSet& validMimetypes);

    FileWriteEventHash m_writeEvents;
    FileReadEventHash m_readEvents;
    HashControl m_hashControl;
    std::unordered_set<gid_t> m_groups;
    uid_t m_uid; // real uid cached
    int m_ourProcFdDirDescriptor; // holds open fd nb for /proc/self/fd
    int m_sizeOfCachedReadFiles;
    QMimeDatabase m_mimedb;
};

