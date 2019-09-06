#include "shell_globals.h"

ShellGlobals &ShellGlobals::instance()
{
    static ShellGlobals s;
    return s;
}

ShellGlobals::ShellGlobals()
{
     ignoreEvents.clear();
     ignoreSigation.clear();
}
