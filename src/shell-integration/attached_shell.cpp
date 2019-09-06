
#include "attached_shell.h"


void AttachedShell::handleEnable()
{}

/// This function is meant to be called only *once*
/// per command sequence.
bool AttachedShell::lastCmdWasValid()
{
    return true;
}
