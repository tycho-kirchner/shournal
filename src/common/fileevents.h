#pragma once

#include <linux/limits.h>

#include "shournalk_user.h"
#include "nullable_value.h"
#include "strlight.h"


class FileEvent {
public:

    int flags() const; /* One of O_RDONLY, O_WRONLY, O_RDWR */
    uint64_t mtime() const;
    size_t size() const;
    uint64_t mode() const;
    HashValue hash() const;
    off_t fileContentSize() const;
    off_t fileContentStart() const;
    const char* path() const;

    FILE *file() const;

private:
    void setPath(const char* path);

    shournalk_close_event m_close_event;
    QByteArray m_path;
    off_t m_fileContentStart;
    FILE* m_file;

    friend class FileEvents;
    friend class DbCtrlTest;
};


/// Write file-events (in binary format) to a log-file and
/// read them later on.
class FileEvents
{
public:
    static bool isReadEvent(int flags);
    static bool isWriteEvent(int flags);

public:
    FileEvents();

    void write(int flags, const StrLight &path,
               const struct stat &st, HashValue hash, int storefd=-1);
    void incrementDropCount(int eventType);

    void clear();

    FileEvent* read();

    FILE *file() const;
    void setFile(FILE *file);

    uint rEventCount() const;
    uint rDroppedCount() const;
    uint rStoredFilesCount() const;

    uint wEventCount() const;
    uint wDroppedCount() const;
    uint wStoredFilesCount() const;



private:
    Q_DISABLE_COPY(FileEvents)

    void writeFilenameToFile(const StrLight& path, bool isREvent);

    FILE* m_file{};
    FileEvent m_fileEvent{};
    shournalk_close_event m_eventTmp{};

    StrLight m_wbuf_lastReadDir;
    StrLight m_wbuf_lastWrittenDir;

    QByteArray m_rbuf_lastReadDir;
    QByteArray m_rbuf_lastWrittenDir;
    char m_pathTmp[PATH_MAX];
    uint m_rEventCount{0};
    uint m_rDroppedCount{0};
    uint m_rStoredFilesCount{0};

    uint m_wEventCount{0};
    uint m_wDroppedCount{0};
    uint m_wStoredFilesCount{0};
};



