
#include <unistd.h>
#include <fstream>
#include <memory.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <fcntl.h>

#include "pidcontrol.h"
#include "logger.h"
#include "os.h"
#include "osutil.h"


/// returns an empty string, if opening fails
/// (or the file belonging to pid was empty)
std::string pidcontrol::parseCmdlineOfPID(pid_t pid)
{
    std::string cmdline;

    const std::string pathToPid = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream f;
    f.open(pathToPid, std::fstream::in);
    if(f.is_open() ) {
        // recombine the string-set to one string. From man proc:
        // The command-line arguments appear
        // in this file as a set of strings separated by null bytes
        // ('\0'), with a further null byte after the last string.
        char ch;
        cmdline.reserve(128);
        bool previousChWasBSlash0 = false;
        while (f >> std::noskipws >> ch) {
            if(ch == '\0'){
                if(previousChWasBSlash0){
                    break;
                }
                cmdline.push_back(' ');
                previousChWasBSlash0 = true;
            } else {
                cmdline.push_back(ch);
                previousChWasBSlash0 = false;
            }
        }
        if(! cmdline.empty()){
            cmdline.pop_back();
        }
    }
    return cmdline;
}





/// Read the status file at /proc/$PID/status and return
/// the real user id found in it (but check for null).
/// See also man 5 proc.
/// @param procDirFd: *must* be an open directory descriptor
/// at /proc/$pid
NullableValue<uid_t>
pidcontrol::parseRealUidOf(int procDirFd){
    std::string uid = osutil::parseGenericKeyValFile(procDirFd, "status", "Uid:");
    if(uid.empty()){
        return {};
    }
    return {qVariantTo_throw<uid_t>(QByteArray::fromStdString(uid))};
}






