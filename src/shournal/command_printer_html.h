#pragma once

#include <unordered_set>
#include <QMimeDatabase>

#include "command_printer.h"
#include "fileinfos.h"
#include "commandinfo.h"
#include "qfilethrow.h"
#include "cmd_stats.h"

class QTextStream;
class QTemporaryFile;

class CommandPrinterHtml : public CommandPrinter
{
public:
    CommandPrinterHtml() = default;

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) override;

protected:
     Q_DISABLE_COPY(CommandPrinterHtml)

    typedef std::unordered_set<qint64> FileReadInfoSet_t;
    void processSingleCommand(QTextStream& outstream, CommandInfo& cmd, QDateTime&
                              finalCommandEndDate, QTemporaryFile& tmpCmdDataFile);
    void writeCmdStartup(const CommandInfo& cmd, QTextStream& outstream);
    void writeCmdData(const CommandInfo& cmd,
                      QTemporaryFile& tmpCmdDataFile);

    void addScriptsToReadFilesSet(const FileReadInfos& infos, FileReadInfoSet_t& set);
    void writeReadFileContentsToHtml(QTextStream& outstream, FileReadInfoSet_t& readFileIdSet);
    void writeFileToStream(QFileThrow& f, QTextStream& outstream);
    void writeStatistics(QTextStream& outstream);

    QMimeDatabase m_mimedb;
    bool m_writeDatesWithMillisec{true};

};


