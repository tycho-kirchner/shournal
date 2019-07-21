#pragma once

#include <QPair>

#include "nullable_value.h"

struct FileWriteEvent{
    time_t mtime;
    off_t  size;
    std::string fullPath;
    HashValue hash;
};

struct FileReadEvent{
    time_t mtime;
    off_t size;
    std::string fullPath;
    mode_t mode;
    QByteArray bytes; // the script file itself
};


typedef QPair<dev_t, ino_t> DevInodePair;
typedef QHash<DevInodePair, FileWriteEvent> FileWriteEventHash;
typedef QHash<DevInodePair, FileReadEvent>  FileReadEventHash;
