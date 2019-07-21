
#include "attached_shell.h"


AttachedShell::AttachedShell()= default;

AttachedShell::~AttachedShell()= default;

void AttachedShell::handleEnable()
{}

/// This function is meant to be called only *once*
/// per command sequence.
bool AttachedShell::lastCmdWasValid()
{
    return true;
}
