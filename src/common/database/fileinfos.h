#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>

#include "nullable_value.h"
#include "db_globals.h"

struct CommandInfo;

struct FileInfo {
    virtual ~FileInfo() = 0;

    qint64 idInDb { db::INVALID_INT_ID };

    QDateTime mtime;
    qint64    size {};
    QString   path;
    QString   name;
    HashValue  hash;

    virtual QString currentStatus(const CommandInfo &cmd) const;
    virtual void write(QJsonObject &json) const = 0;
    virtual bool operator==(const FileInfo& rhs) const = 0 ;
};

struct FileWriteInfo : public FileInfo
{

    virtual void write(QJsonObject &json) const;
    virtual bool operator==(const FileInfo& rhs) const;

};


struct FileReadInfo : public FileInfo
{
    mode_t mode {};
    bool isStoredToDisk {false};

    virtual void write(QJsonObject &json) const;

    virtual bool operator==(const FileReadInfo& rhs) const;
    virtual bool operator==(const FileInfo& rhs) const;
};
