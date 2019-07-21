#pragma once

#include <memory>
#include <QMimeDatabase>

#include "storedfiles.h"
#include "user_str_conversions.h"


class CommandQueryIterator;
class QFormattedStream;

/// Print command-infos and corresponding file events to stdout.
/// Since we can potentially stream only once over the sql-result,
/// restore read files one the fly, if configured so.
class PrettyPrint {
public:
    PrettyPrint();

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter);

    void setMaxCountOfReadFileLines(int maxCountOfReadFileLines);

    void setRestoreReadFiles(bool restoreReadFiles);

    void setRestoreDir(const QDir &restoreDir);

private:
    void printReadFileEventEvtlRestore(QFormattedStream& s, const FileReadInfo& readInfo,
                                  const QString& cmdIdStr);
    void restoreReadFile(const FileReadInfo& readInfo,
                         const QString &cmdIdStr, const QFile &openReadFile);
    void printReadFile(QFormattedStream& s, QFile& f);

    QMimeDatabase m_mimedb;
    StoredFiles m_storedFiles;
    const QString m_indentlvl0;
    const QString m_indentlvl1;
    const QString m_indentlvl2;
    const QString m_indentlvl3;
    int m_maxCountOfReadFileLines;
    bool m_restoreReadFiles;
    int m_countOfRestoredFiles;
    QDir m_restoreDir;
    UserStrConversions m_userStrConv;

};





