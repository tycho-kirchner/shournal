#pragma once

#include "command_printer.h"
#include "commandinfo.h"

#include <QMimeDatabase>


class CommandPrinterHuman : public CommandPrinter
{
public:
    CommandPrinterHuman() = default;

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) override;

    virtual void setMaxCountOfReadFileLines(int maxCountOfReadFileLines);

protected:
    Q_DISABLE_COPY(CommandPrinterHuman)

    void printReadFileEventEvtlRestore(QFormattedStream& s, const FileReadInfo& readInfo,
                                       const QString& cmdIdStr);
    void printReadFile(QFormattedStream& s, QFile& f);

    void printWriteInfos(QFormattedStream& s, const FileWriteInfos& fileWriteInfos);
    void printReadInfos(QFormattedStream& s, const CommandInfo& cmd);

    QMimeDatabase m_mimedb;
    const QString m_indentlvl0 {""};
    const QString m_indentlvl1 {"  "};
    const QString m_indentlvl2 {"     "};
    const QString m_indentlvl3 {"          "};
    int m_maxCountOfReadFileLines {5};
    Conversions m_userStrConv;
};
