#pragma once

#include <unistd.h>

namespace msenter {

[[noreturn]]
void run(pid_t targetPid, const char *filename, char *commandArgv[], char **envp);
[[noreturn]]
void run(const char* filename, char *commandArgv[], char **envp,
         int targetprocDirFd);

}



