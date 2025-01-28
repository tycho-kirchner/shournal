#pragma once

#include <memory>

#include "storedfiles.h"
#include "conversions.h"
#include "qfilethrow.h"
#include "cmd_stats.h"


class CommandQueryIterator;
class QFormattedStream;

/// Base class for command-printers (human, json).
/// Print command-infos and corresponding file events to stdout.
/// Since we can potentially stream only once over the sql-result,
/// restore read files on the fly, if configured so.
class CommandPrinter
{
public:
    CommandPrinter();
    virtual ~CommandPrinter() = default;

    virtual void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) = 0;
    virtual void setRestoreReadFiles(bool restoreReadFiles);
    virtual void setRestoreDir(const QDir &restoreDir);
    virtual QFileThrow& outputFile();
    virtual void setQueryString(const QString &queryString);
    virtual void setMaxCountWfiles(int maxCountWfiles);
    virtual void setMaxCountRfiles(int maxCountRfiles);
    virtual CmdStats& cmdStats();
    virtual void setMinCountOfStats(int val);
    virtual void setReportFileStatus(bool val);
    virtual bool reportFileStatus() const;

protected:
    Q_DISABLE_COPY(CommandPrinter)

    void createRestoreTopleveDirIfNeeded();

    void restoreReadFile_safe(const FileReadInfo& readInfo,
                         const QString &cmdIdStr);
    void restoreReadFile_safe(const FileReadInfo& readInfo,
                         const QString &cmdIdStr, const QFile &openReadFile);


    StoredFiles m_storedFiles;
    bool m_restoreReadFiles {false};
    int m_countOfRestoredFiles {0};
    QDir m_restoreDir;
    QFileThrow m_outputFile;
    QString m_queryString; // entered by user on commandline
    int m_maxCountWfiles{0}; // do not print more than that number of written files per command
    int m_maxCountRfiles{0};
    CmdStats m_cmdStats;
    int m_minCountOfStats;
    bool m_reportFileStatus{false};
};





