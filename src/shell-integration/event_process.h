#pragma once

#include <sched.h>

namespace event_process {

pid_t handleFork();

int handleExecve(const char *filename, char *const argv[],
               char *const envp[]);
}

