#pragma once

#include <sys/stat.h>
#include <linux/limits.h>

#include "nullable_value.h"
#include "util.h"
#include "strlight.h"


/// These structures are used to temporarily and efficiently store metadata of the
/// collected file events to disk. We cannot reliably write to the
/// database during event processing, as this might take too long
/// (so events could get lost). Further, doing so, might impact
/// the overall system performance (that's why the later storing
/// happens with a small process priority).
/// Events are collected using write(). Before calling read(),
/// fseekToBegin() *must* have been called beforehand.

// Must have same structure as
// FileWriteEventInternal (see FileWriteEvents::read)
struct FileWriteEvent {
    time_t mtime;
    off_t size;
    uint64_t hash;
    bool hashIsNull;
    // must be at end
    char fullPath[PATH_MAX]; // #include <linux/limits.h>
};
static_assert (std::is_pod<FileWriteEvent>(), "");


class FileWriteEvents {
public:
    FileWriteEvents(const char* parentTempDir);
    ~FileWriteEvents();
    void write(const StrLight& path, const struct stat &st,
                   HashValue hash );
    FileWriteEvent * read();

    void clear();
    void fseekToBegin();

private:
    Q_DISABLE_COPY(FileWriteEvents)

    // Must have same structure as
    // FileWriteEvent (see FileWriteEvents::read)
    struct FileWriteEventInternal {
        time_t mtime;
        off_t size;
        uint64_t hash;
        bool hashIsNull;
    };
    static_assert (std::is_pod<FileWriteEventInternal>(), "");

    FILE* m_file;
    FileWriteEventInternal m_eventTmp;
    FileWriteEvent m_currentEvent;
    bool m_readAllowed{false};
};

/////////////////////////////////////////////////////////////

// Must have same structure as
// FileReadEventInternal (see FileReadEvents::read)
struct FileReadEvent {
    time_t mtime;
    off_t size;
    mode_t mode;
    uint64_t hash;
    bool hashIsNull;
    // if id != -1, the file is stored on disk
    int file_content_id;
     // must be at end
    char fullPath[PATH_MAX]; // #include <linux/limits.h>

};
static_assert (std::is_pod<FileReadEvent>(), "");

/// If a file shall be stored within our database, it is also cached
/// beforehand within the temp dir. If FileReadEvent.file_content_id != -1
/// you may call FileReadEvents::makeStoredFilepath to obtain the full path
/// to the beloning stored file.
class FileReadEvents {
public:
    FileReadEvents(const char* parentTempDir);
    ~FileReadEvents();
    void write(const StrLight& path, const struct stat &st,
                   HashValue hash, int fd, bool store);
    FileReadEvent* read();

    void fseekToBegin();

    void clear();

    QByteArray makeStoredFilepath(const FileReadEvent& e);

    uint getStoredFilesCounter() const;

    const QByteArray& getTmpDirPath() const;

private:
    Q_DISABLE_COPY(FileReadEvents)

    // Must have same structure as
    // FileReadEvent (see FileReadEvents::read)
    struct FileReadEventInternal {
        time_t mtime;
        off_t size;
        mode_t mode;
        uint64_t hash;
        bool hashIsNull;
        int file_content_id;
    };

    FILE* m_file;
    int m_tmpDirFd;
    FileReadEventInternal m_eventTmp;
    FileReadEvent m_currentEvent;
    const QByteArray m_tmpDirPath;
    uint m_storedFilesCounter;
    bool m_readAllowed{false};

    QByteArray makeStoredFilename(int number);
    uint storeFile(int fd, const struct stat &st);

};

