#include "cmd_stats.h"

#include "cleanupresource.h"

/// Do not collect more than that many entries of each category
CmdStats::CmdStats() :
    m_maxCountOfStats(5)
{
    m_cmdsWithMostFileModsQueue.setMaxSize(m_maxCountOfStats);
}

void CmdStats::setMaxCountOfStats(const int &val)
{
    m_maxCountOfStats = val;
    m_cmdsWithMostFileModsQueue.setMaxSize(val);
}

void CmdStats::collectCmd(const CommandInfo &cmd)
{
    auto incrementIdxLater = finally([this] { ++m_currentCmdIdx; });

    if(! cmd.fileWriteInfos.isEmpty()){
        MostFileModsEntry mostFileMods;
        mostFileMods.idx = m_currentCmdIdx;
        mostFileMods.idInDb = cmd.idInDb;
        mostFileMods.cmdTxt = cmd.text;
        mostFileMods.countOfFileMods = cmd.fileWriteInfos.size();
        m_cmdsWithMostFileModsQueue.push(mostFileMods);
    }

    if(! cmd.sessionInfo.uuid.isNull()){
        auto & el = m_sessionMostCmdsMap[cmd.sessionInfo.uuid];
        if(el.idx == -1){
            // remember the first cmd in this session
            el.idx = m_currentCmdIdx;
            el.idInDb = cmd.idInDb;
        }
        el.cmdUuid = cmd.sessionInfo.uuid;
        ++el.cmdCount;
    }

    {
        auto & cwdCmdCountEntry = m_cwdCmdCountMap[cmd.workingDirectory];
        ++cwdCmdCountEntry.cmdCount;
    }

    for(const auto& info : cmd.fileReadInfos){
        auto & dirIoEntry = m_dirIoCountMap[info.path];
        ++dirIoEntry.readCount;
    }
    for(const auto& info : cmd.fileWriteInfos){
        auto & dirIoEntry = m_dirIoCountMap[info.path];
        ++dirIoEntry.writeCount;
    }
}

/// aggregate the collected commands -> meant to be called, after all commands
/// were collected. This function may be called only once as it clears
/// afterwards not needed data.
void CmdStats::eval()
{
    m_cmdsWithMostFileMods = m_cmdsWithMostFileModsQueue.popAll<MostFileModsEntrys>(true);

    limited_priority_queue<SessionMostCmdsEntry,
                           SessionMostCmds,
                           cmpSessionMostCmdEntry> sessionMostCmdsPq;
    sessionMostCmdsPq.setMaxSize(m_maxCountOfStats);
    for(const auto & el : m_sessionMostCmdsMap){
        sessionMostCmdsPq.push(el);
    }

    m_sessionMostCmds = sessionMostCmdsPq.popAll<SessionMostCmds>(true);
    m_sessionMostCmdsMap.clear();


    limited_priority_queue<CwdCmdCount, CwdCmdCounts, cmpCwdCmdCount> cwdCmdCountQueue;
    cwdCmdCountQueue.setMaxSize(m_maxCountOfStats);
    for(auto it=m_cwdCmdCountMap.begin(); it != m_cwdCmdCountMap.end(); ++it){
        // was not yet assigned because here it has to be assigned only once per
        // working dir
        it.value().workingDir = it.key();
        cwdCmdCountQueue.push(it.value());
    }
    m_cwdCmdCounts = cwdCmdCountQueue.popAll<CwdCmdCounts>(true);
    m_cwdCmdCountMap.clear();


    limited_priority_queue<DirIoCount, DirIoCounts, cmpDirIoCount> dirIoCountQueue;
    dirIoCountQueue.setMaxSize(m_maxCountOfStats);
    for(auto it=m_dirIoCountMap.begin(); it != m_dirIoCountMap.end(); ++it){
        it.value().dir = it.key();
        dirIoCountQueue.push(it.value());
    }
    m_dirIoCounts = dirIoCountQueue.popAll<DirIoCounts>(true);
    m_dirIoCountMap.clear();
}

const CmdStats::MostFileModsEntrys &CmdStats::cmdsWithMostFileMods() const
{
    return m_cmdsWithMostFileMods;
}

const CmdStats::SessionMostCmds &CmdStats::sessionMostCmds() const
{
    return m_sessionMostCmds;
}

const CmdStats::CwdCmdCounts &CmdStats::cwdCmdCounts() const
{
    return m_cwdCmdCounts;
}

const CmdStats::DirIoCounts &CmdStats::dirIoCounts() const
{
    return m_dirIoCounts;
}
