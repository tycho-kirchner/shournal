#pragma once

#include <memory>
#include "storedfiles.h"
#include "user_str_conversions.h"


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

protected:
    void createRestoreTopleveDirIfNeeded();

    void restoreReadFile_safe(const FileReadInfo& readInfo,
                         const QString &cmdIdStr);
    void restoreReadFile_safe(const FileReadInfo& readInfo,
                         const QString &cmdIdStr, const QFile &openReadFile);

    StoredFiles m_storedFiles;
    bool m_restoreReadFiles {false};
    int m_countOfRestoredFiles {0};
    QDir m_restoreDir;
};





