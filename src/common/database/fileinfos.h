#pragma once

#include <QString>
#include <QDateTime>

#include "nullable_value.h"
#include "db_globals.h"


struct FileWriteInfo
{
    QDateTime mtime;
    qint64    size {};
    QString   path;
    QString   name;
    HashValue  hash;

    bool operator==(const FileWriteInfo& rhs) const;
};


struct FileReadInfo
{
    FileReadInfo() = default;

    qint64 idInDb { db::INVALID_INT_ID };

    QDateTime mtime;
    qint64 size {};
    QString path;
    QString name;
    mode_t mode {};
    HashValue hash;
    bool isStoredToDisk {false};

    bool operator==(const FileReadInfo& rhs) const;
};
