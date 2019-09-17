
#include <QDebug>
#include <dlfcn.h>


#include "attached_bash.h"
#include "os.h"

/// @throws ExcOs
AttachedBash::AttachedBash() :
    m_pExternCurrentCmdNumber(
        reinterpret_cast<int*>(os::dlsym(RTLD_DEFAULT, "current_command_number"))
        ),
    m_lastCmdNumber(1) // see bash sourcecode shell.c, current_command_number is initialized to 1
{}

void AttachedBash::handleEnable()
{
     m_lastCmdNumber = *m_pExternCurrentCmdNumber;
}

/// The command is considered valid, if the command-counter
/// has changed since the last call of this function or handleEnable().
/// This function is meant to be called only *once*
/// per command sequence.
bool AttachedBash::cmdCounterJustIncremented()
{
    if(*m_pExternCurrentCmdNumber == m_lastCmdNumber){
        return false;
    }
    m_lastCmdNumber = *m_pExternCurrentCmdNumber;
    return true;
}
