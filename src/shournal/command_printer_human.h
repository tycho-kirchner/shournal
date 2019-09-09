#pragma once

#include "command_printer.h"

#include <QMimeDatabase>


class CommandPrinterHuman : public CommandPrinter
{
public:

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) override;

    virtual void setMaxCountOfReadFileLines(int maxCountOfReadFileLines);

protected:
    void printReadFileEventEvtlRestore(QFormattedStream& s, const FileReadInfo& readInfo,
                                       const QString& cmdIdStr);
    void printReadFile(QFormattedStream& s, QFile& f);

    QMimeDatabase m_mimedb;
    const QString m_indentlvl0 {""};
    const QString m_indentlvl1 {"  "};
    const QString m_indentlvl2 {"     "};
    const QString m_indentlvl3 {"          "};
    int m_maxCountOfReadFileLines {5};
    UserStrConversions m_userStrConv;
};
