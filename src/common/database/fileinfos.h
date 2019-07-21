#pragma once

#include <QString>
#include <QDateTime>

#include "nullable_value.h"


struct FileWriteInfo
{
    FileWriteInfo();

    QDateTime mtime;
    qint64    size;
    QString   path;
    QString   name;
    HashValue  hash;

    bool operator==(const FileWriteInfo& rhs) const;
};


struct FileReadInfo
{
    FileReadInfo();

    qint64 idInDb;

    QDateTime mtime;
    qint64 size;
    QString path;
    QString name;
    mode_t mode;

    bool operator==(const FileReadInfo& rhs) const;
};
