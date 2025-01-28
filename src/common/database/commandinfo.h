#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>

#include "hashmeta.h"
#include "sessioninfo.h"
#include "fileinfos.h"


typedef QVector<FileWriteInfo> FileWriteInfos;
typedef QVector<FileReadInfo> FileReadInfos;

/// Configure which fields shall be written
/// to JSON on CommandInfo.write() (and how many
/// entries of some fields)
struct CmdJsonWriteCfg {
    CmdJsonWriteCfg(bool initAll) :
        idInDb(initAll),
        text(initAll),
        returnVal(initAll),
        username(initAll),
        hostname(initAll),
        hashMeta(initAll),
        sessionInfo(initAll),
        startEndTime(initAll),
        workingDirectory(initAll),
        fileWriteInfos(initAll),
        fileReadInfos(initAll)
    {}

    bool idInDb;
    bool text;
    bool returnVal;
    bool username;
    bool hostname;
    bool hashMeta;
    bool sessionInfo;

    bool startEndTime;
    bool workingDirectory;

    bool fileWriteInfos;
    bool fileReadInfos;

    int maxCountWFiles{std::numeric_limits<int>::max()};
    int maxCountRFiles{std::numeric_limits<int>::max()};

    bool fileStatus{false};
};

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

    QDateTime startTime;
    QDateTime endTime;
    QString workingDirectory;

    FileWriteInfos fileWriteInfos;
    FileReadInfos fileReadInfos;

    void write(QJsonObject &json, bool withMilliseconds=false,
               const CmdJsonWriteCfg& writeCfg=CmdJsonWriteCfg(true)) const;

    bool operator==(const CommandInfo& rhs) const;

    void clear();

};

