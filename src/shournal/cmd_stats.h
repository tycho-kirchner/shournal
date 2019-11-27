#pragma once

#include <QVector>
#include <QHash>

#include "commandinfo.h"
#include "limited_priority_queue.h"

class CmdStats
{
public:
    // Commands which modified the most files
    struct MostFileModsEntry {
        int idx; // zero based collectCmd index (first command -> 0...)
        qint64 idInDb;
        QString cmdTxt;
        int countOfFileMods;
    };

    // Sessions where the most commands where executed in
    struct SessionMostCmdsEntry {
        int idx {-1}; // idx of the first cmd of this session
        qint64 idInDb{-1}; // id of the first cmd of this session
        int cmdCount{0}; // number of commands executed in this session
        QByteArray cmdUuid;
    };

    // Count of commands executed in
    // a specific CurrentWorkingDirectory
    struct CwdCmdCount {
        QString workingDir;
        int cmdCount{0};
    };

    // Directories, where the most files were read and written
    struct DirIoCount {
        QString dir;
        qint64 readCount{0};
        qint64 writeCount{0};
    };

    typedef QVector<MostFileModsEntry> MostFileModsEntrys;
    typedef QVector<SessionMostCmdsEntry> SessionMostCmds;
    typedef QVector<CwdCmdCount> CwdCmdCounts;
    typedef QVector<DirIoCount> DirIoCounts;

public:

    CmdStats();

    void setMaxCountOfStats(const int &val);

    void collectCmd(const CommandInfo& cmd);

    void eval();

    const MostFileModsEntrys& cmdsWithMostFileMods() const;

    const SessionMostCmds& sessionMostCmds() const;

    const CwdCmdCounts& cwdCmdCounts() const;

    const DirIoCounts& dirIoCounts() const;

private:
    struct cmpFileModEntry {
        bool operator()(const MostFileModsEntry & e1, const MostFileModsEntry & e2) {
            return e1.countOfFileMods > e2.countOfFileMods;
        }
    };

    struct cmpSessionMostCmdEntry {
        bool operator()(const SessionMostCmdsEntry & e1, const SessionMostCmdsEntry & e2) {
            return e1.cmdCount > e2.cmdCount;
        }
    };

    struct cmpCwdCmdCount {
        bool operator()(const CwdCmdCount & e1, const CwdCmdCount & e2) {
            return e1.cmdCount > e2.cmdCount;
        }
    };

    struct cmpDirIoCount {
        bool operator()(const DirIoCount & e1, const DirIoCount & e2) {
            return e1.readCount + e1.writeCount > e2.readCount + e2.writeCount;
        }
    };
    limited_priority_queue<MostFileModsEntry,
                           MostFileModsEntrys,
                           cmpFileModEntry> m_cmdsWithMostFileModsQueue;
    MostFileModsEntrys m_cmdsWithMostFileMods;

    QHash<QByteArray, SessionMostCmdsEntry> m_sessionMostCmdsMap;
    SessionMostCmds m_sessionMostCmds;

    QHash<QString, CwdCmdCount> m_cwdCmdCountMap;
    CwdCmdCounts m_cwdCmdCounts;

    QHash<QString, DirIoCount> m_dirIoCountMap;
    DirIoCounts m_dirIoCounts;


    int m_maxCountOfStats;
    int m_currentCmdIdx{0};
};

