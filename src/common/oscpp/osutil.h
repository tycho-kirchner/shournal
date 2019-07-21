#pragma once

#include <QDebug>
#include <sys/resource.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "os.h"
#include "cleanupresource.h"
#include "util.h"

namespace osutil {

int countOpenFds();

rlim_t getMaxCountOpenFiles();

template <class Str_t>
Str_t findPathOfFd(int fd);


void printOpenFds(bool onlyRegular=false);

bool fdIsOpen(int fd);

bool sameFile(const os::stat_t& st1, const os::stat_t& st2);

int retrieveFdFlags(int fd);
int retrieveFdFlags(int fdInfoDir, const std::string &fdNb);

int reopenFdByPath(int oldFd, int openflags, bool clo_exec=true,
                   bool restoreOffset=true);

std::string parseGenericKeyValFile(int dirFd,
                                   const std::string &filename,
                                   const std::string &key);

std::string fcntlflagsToString(int flags);


QByteArray readWholeFile(int fd, int bufSize);

bool isTTYForegoundProcess(int fd);

void waitForSignals();

void closeVerbose(int fd);


} // namespace fdcontrol





/// Find path where an open fd of OUR process (currently) points to.
/// @return path
/// @throws ExcReadLink
template <class Str_t>
Str_t osutil::findPathOfFd(int fd){
    char procfdPath[PATH_MAX];
    snprintf(procfdPath, sizeof(procfdPath), "/proc/self/fd/%d", fd);
    Str_t path = os::readlink<Str_t>(procfdPath);
    return path;
}




