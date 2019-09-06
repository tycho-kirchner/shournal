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
void closeVerbose(int fd);

std::string fcntlflagsToString(int flags);
bool fdIsOpen(int fd);

/// Shells usually start at low numbers for internal file descriptors (usually 10),
/// we try to find the highest possible free fd
/// If startFd != -1, start searching from that.
int findHighestFreeFd(int startFd=-1, int minFd=11);


template <class Str_t>
Str_t findPathOfFd(int fd);

rlim_t getMaxCountOpenFiles();

bool isTTYForegoundProcess(int fd);


std::string parseGenericKeyValFile(int dirFd,
                                   const std::string &filename,
                                   const std::string &key);
void printOpenFds(bool onlyRegular=false);


QByteArray readWholeFile(int fd, int bufSize);
int retrieveFdFlags(int fd);
int retrieveFdFlags(int fdInfoDir, const std::string &fdNb);
int reopenFdByPath(int oldFd, int openflags, bool clo_exec=true,
                   bool restoreOffset=true);

bool sameFile(const os::stat_t& st1, const os::stat_t& st2);

void waitForSignals();




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




