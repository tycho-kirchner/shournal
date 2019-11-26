#include <cassert>

#include "event_other.h"

#include "shell_globals.h"
#include "cleanupresource.h"

/// strcpy is LD_PRELOAD'ed because I could not find
/// a reliable way to find out the start-time of a bash-command.
/// Here we rely on current_command_number being incremented
/// and strcpy being called right *before* the first simple command is executed.
char *event_other::handleStrcpy(char *dest, const char *src)
{
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_strcpy(dest, src);
    }
    auto clearIgnEvents = finally([&g_shell] { g_shell.ignoreEvents.clear(); });

    if(! g_shell.inSubshell &&
         g_shell.watchState == E_WatchState::WITHIN_CMD &&
         g_shell.pAttchedShell->cmdCounterJustIncremented()){

        // lastCmdStartTime is usually null, except shournal-run fails
        // to launch. Overwrite anyway.
        g_shell.lastCmdStartTime = QDateTime::currentDateTime();
    }
    return g_shell.orig_strcpy(dest, src);
}
