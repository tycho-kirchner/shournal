
// necessary for RTLD_NEXT in dlfcn.h
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <exception>
#include <cstdio>
#include <iostream>

#include "event_open.h"
#include "event_other.h"
#include "event_process.h"
#include "staticinitializer.h"
#include "shell_globals.h"
#include "logger.h"
#include "qoutstream.h"
#include "shell_logger.h"
#include "translation.h"
#include "app.h"

// cmake export-symbol control:
#include "libshournal-shellwatch_export.h"

namespace  {

/// Initalize the original functions close, fclose ...
/// One might think it was a good idea, to initialize the functions
/// in gcc's __attribute__((constructor)). This is too late,
/// at that time fclose/close was already called several times.
void initSymIfNeeded(){
    static StaticInitializer loader( [](){
        try {
            ShellGlobals& globals = ShellGlobals::instance();
            globals.orig_fork = reinterpret_cast<fork_func_t>(os::dlsym(RTLD_NEXT, "fork"));
            globals.orig_execve = reinterpret_cast<execve_func_t>(os::dlsym(RTLD_NEXT, "execve"));
            globals.orig_open = reinterpret_cast<open_func_t>(os::dlsym(RTLD_NEXT, "open"));
            // globals.orig_fopen = reinterpret_cast<fopen_func_t>(os::dlsym(RTLD_NEXT, "fopen"));
            globals.orig_strcpy = reinterpret_cast<strcpy_func_t>(os::dlsym(RTLD_NEXT, "strcpy"));

            return;
        } catch(const os::ExcOs& ex){
            logCritical << "failed to load original symbols, expect the worst..." << ex.what();
            throw ;
        }
    });
}


} // namespace

#ifdef __cplusplus
extern "C" {
#endif


LIBSHOURNAL_SHELLWATCH_EXPORT
int open(const char *pathname, int flags, mode_t mode) {
    // std::cerr << __func__ << "\n";
    initSymIfNeeded();
    try{
        return event_open::handleOpen(pathname, flags, mode, false);
    } catch (const std::exception& ex ) {
        std::cerr << __func__ << " fatal: " << ex.what() << "\n";
    }
    return ShellGlobals::instance().orig_open(pathname, flags, mode);
}


LIBSHOURNAL_SHELLWATCH_EXPORT
int open64(const char *pathname, int flags, mode_t mode) {
    // std::cerr << __func__ << "\n";
    initSymIfNeeded();
    try{
        // probably O_LARGEFILE should only be set, if we are running in 32
        // bit mode (using open64). It seems to do no harm though (see handleOpen).
        return event_open::handleOpen(pathname, flags, mode, true);
    } catch (const std::exception& ex ) {
        std::cerr << __func__ << " fatal: " << ex.what() << "\n";
    }
    return ShellGlobals::instance().orig_open(pathname, flags, mode);
}

// There seems to be no point in observing fopen - browsing the source-code
// of bash, zsh, kash, csh, .. all relevant user file activity is handled via
// the 'open' library-call. If one day it would be observed anyway: the shell's
// seem to not make use of any (g)libc-fopen-mode-extensions like 'c' or ,ccs=string.
// As such the translation of w,r,a etc. to O_WRONLY,O_RDONLY, etc. is pretty
// straight forward. Otherwise things get more complicated - fdopen does not handle
// all the cases.
// LIBSHOURNAL_SHELLWATCH_EXPORT
// FILE* fopen(const char *path, const char *mode) {
//     // std::cerr << __func__ << "\n";
//     try{
//         initSymIfNeeded();
//         return event_open::handleFopen(path, mode);
//     } catch (const std::exception& ex ) {
//         std::cerr << __func__ << " fatal: " << ex.what() << "\n";
//     }
//     return nullptr;
// }


// see comment for fopen.
// LIBSHOURNAL_SHELLWATCH_EXPORT
// FILE* fopen64(const char *path, const char *mode) {
//     // initIfNeeded();
//     // FILE* f = orig_fopen64(path, mode);
//     // if(f != NULL){
//     //     handleOpen(fileno(f));
//     // }
//     return f;
// }




LIBSHOURNAL_SHELLWATCH_EXPORT
pid_t fork(){
    initSymIfNeeded();
    try {
        return event_process::handleFork();
    } catch (const std::exception& ex ) {
        std::cerr << __func__ << " fatal: " << ex.what() << "\n";
    }
    return ShellGlobals::instance().orig_fork();

}

LIBSHOURNAL_SHELLWATCH_EXPORT
int execve(const char *filename, char *const argv[],
           char *const envp[]){
    initSymIfNeeded();
    try {
        return event_process::handleExecve(filename, argv, envp);
    } catch (const std::exception& ex ) {
        std::cerr << __func__ << " fatal: " << ex.what() << "\n";
    }
    return ShellGlobals::instance().orig_execve(filename, argv, envp);
}


LIBSHOURNAL_SHELLWATCH_EXPORT
char *strcpy(char *dest, const char *src){
    initSymIfNeeded();
    try {
        return event_other::handleStrcpy(dest, src);
    } catch (const std::exception& ex ) {
        std::cerr << __func__ << " fatal: " << ex.what() << "\n";
    }
    return ShellGlobals::instance().orig_strcpy(dest, src);
}

#ifdef __cplusplus
}
#endif
