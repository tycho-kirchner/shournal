#pragma once

#include <QString>
#include <QVector>

#include "hashmeta.h"
#include "sessioninfo.h"
#include "fileinfos.h"


typedef QVector<FileWriteInfo> FileWriteInfos;
typedef QVector<FileReadInfo> FileReadInfos;

struct CommandInfo
{
    // Invalid return value set, if no return value could be determined (e.g. because
    // the shell-process called execve() before fork
    static const qint32 INVALID_RETURN_VAL = std::numeric_limits<qint32>::max();

    static CommandInfo fromLocalEnv();

    CommandInfo();
    qint64 idInDb;
    QString text;
    qint32 returnVal;
    QString username;
    QString hostname;
    HashMeta hashMeta;
    SessionInfo sessionInfo;

    FileWriteInfos fileWriteInfos;
    FileReadInfos fileReadInfos;

    QDateTime startTime;
    QDateTime endTime;
    QString workingDirectory;

    bool operator==(const CommandInfo& rhs) const;

    void clear();

};

