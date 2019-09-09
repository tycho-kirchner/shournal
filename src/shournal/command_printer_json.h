#pragma once

#include "command_printer.h"

class CommandPrinterJson : public CommandPrinter
{
public:

    void printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator>& cmdIter) override;
};

