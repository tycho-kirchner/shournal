#pragma once


namespace orig_mountspace_process {

    void setupIfNotExist();
    [[noreturn]]
    void msenterOrig(const char* filename, char *commandArgv[], char **envp);

}


