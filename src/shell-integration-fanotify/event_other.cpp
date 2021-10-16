#include <cassert>

#include "attached_bash.h"
#include "cleanupresource.h"
#include "event_other.h"
#include "shell_globals.h"
#include "shell_request_handler.h"


/// strcpy is LD_PRELOAD'ed to monitor bash's current_command_number:
/// when it is incremented, we start observing (if we are enabled).
/// We cannot use PS0 (where it is available as \#) easily as it is
/// executed in a subshell.
char *event_other::handleStrcpy(char *dest, const char *src)
{
    auto& g_shell = ShellGlobals::instance();
    if(g_shell.ignoreEvents.test_and_set()){
        return g_shell.orig_strcpy(dest, src);
    }
    auto clearIgnEvents = finally([&g_shell] { g_shell.ignoreEvents.clear(); });

    if(! g_shell.inSubshell &&
         g_shell.watchState == E_WatchState::INTERMEDIATE &&
          dynamic_cast<const AttachedBash*>(g_shell.pAttchedShell) != nullptr &&
         g_shell.pAttchedShell->cmdCounterJustIncremented()){
        shell_request_handler::handlePrepareCmd();
    }
    return g_shell.orig_strcpy(dest, src);
}
