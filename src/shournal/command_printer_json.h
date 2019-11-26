#pragma once

#include "command_printer.h"

class CommandPrinterJson : public CommandPrinter
{
public:
    CommandPrinterJson() = default;

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) override;

private:
    Q_DISABLE_COPY(CommandPrinterJson)
};

