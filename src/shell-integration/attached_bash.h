#pragma once

#include "attached_shell.h"

/// I was unable to find a reliable way to find out, whether the
/// last entered command was empty (invalid) from within the bash_integration.sh.
/// Especially, when IgnoreDups is on, or the history get reloaded from file
/// (at each PROMPT_COMMAND). Therefor we fetch the bash-internal counter
/// 'current_command_number' at runtime, which is only incremented in case of
/// non-empty commands and is independent of history-reloads.
/// The symbol-name has been stable for at least a decade as of March 2019.
class AttachedBash : public AttachedShell
{
public:
    AttachedBash();

    void handleEnable() override;

    bool lastCmdWasValid() override;

private:
    int *m_pExternCurrentCmdNumber;
    int m_lastCmdNumber;
};

