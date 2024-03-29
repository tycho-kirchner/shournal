


#include <cassert>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/resource.h>
#include <sys/time.h>
#include <string>
#include <QDir>
#include <QFileInfoList>
#include <poll.h>
#include <sys/ioctl.h>
#include <fstream>
#include <string>
#include <sstream>
#include <fcntl.h>
#include <ext/stdio_filebuf.h>
#include <thread>
#include <random>
#include <chrono>

#include "osutil.h"
#include "os.h"
#include "pidcontrol.h"
#include "qoutstream.h"
#include "fdentries.h"

#ifdef __cplusplus
extern "C" {
#endif

void intertSighandler(int){}

#ifdef __cplusplus
}
#endif

int osutil::countOpenFds() {
     int count = 0;
     for(const int fd : osutil::FdEntries()){
         Q_UNUSED(fd)
         count++;
     }
     return count;
}


rlim_t osutil::getMaxCountOpenFiles()
{
    struct rlimit rlim;
    getrlimit(RLIMIT_NOFILE, &rlim);
    return rlim.rlim_cur;
}


/// @return true, if fd existed within this process
bool osutil::fdIsOpen(int fd)
{
    const std::string fdpath = "/proc/self/fd/" + std::to_string(fd);
    return os::exists(fdpath);
}


/// @return true, if st1 and st2 refer to the same file (device/inode)
bool osutil::sameFile(const os::stat_t& st1, const os::stat_t& st2)
{
    return st1.st_dev == st2.st_dev &&
            st1.st_ino == st2.st_ino;
}


/// Get the file access mode, file status flags and *some* 'file creation flags' using
/// /proc/$pid/fdinfo/$fd.
/// The returned flags include O_CLOEXEC.
/// @param fdInfoDir: an open directory descritor pointing to an fdinfo-dir.
/// @param fdNb: the file descritor
/// See also: man 5 proc
int osutil::retrieveFdFlags(int fdInfoDir, const std::string& fdNb)
{
    // Note that fcntl(fd, F_GETFL) does *not* return O_CLOEXEC (and possibly others?).
    // That flag would have to be obtained *indirectly* by
    // fcntl(fd , F_GETFD), which, as of March 2019, only has the FD_CLOEXEC-flag
    // (which has a different value than O_CLOEXEC).
    std::string octalFlags = parseGenericKeyValFile(fdInfoDir, fdNb, "flags:");
    return std::stoi( octalFlags, nullptr, 8 );
}


/// Get the file access mode, file status flags and *some* 'file creation flags' using fcntl.
/// The returned flags include O_CLOEXEC (if set).
int osutil::retrieveFdFlags(int fd)
{
    // Note that fcntl(fd, F_GETFL) does *not* return O_CLOEXEC.
    // That flag can be obtained *indirectly* by
    // fcntl(fd , F_GETFD), which, as of March 2019, only has the FD_CLOEXEC-flag
    // (which has a different value than O_CLOEXEC).
    int statusFlags = os::getFdStatusFlags(fd);
    int descrFlags = os::getFdDescriptorFlags(fd);
    if(IsBitSet(descrFlags, FD_CLOEXEC)){
        setBitIn(statusFlags, O_CLOEXEC);
    }
    return statusFlags;
}


/// Reopen an open file decriptor of *this* process by resolving the symlink
/// /proc/self/fd/$fd points to and passing that path (string) to open(2).
/// Make sure that the new file descriptor really refers to the
/// *same* file (it might not be the same path though but instead
/// another hardlink, which is ignored here).
/// @return the new file descriptor
/// @throws ExcOs, especially, if the reopened file has a different device-
/// inode-combination.
int osutil::reopenFdByPath(int oldFd, int openflags, bool clo_exec,
                   bool restoreOffset) {
    const auto oldStat = os::fstat(oldFd);

    // Race condition in next line...
    const int newFd = os::open(osutil::findPathOfFd<QByteArray>(oldFd), openflags, clo_exec);
    // Note: the following is *not* a race-free variant of above call, because after unsharing
    // the mount-namespace, such an open call results to an fd still belonging to the original
    // mnt_id.
    // const int newFd = os::open("/proc/self/fd/" + std::to_string(oldFd), openflags, clo_exec);

    auto closeOnErr = finally([&newFd] { close(newFd); });
    const auto newStat = os::fstat(newFd);
    if(! osutil::sameFile(oldStat, newStat)){
        throw os::ExcOs("reopen failed, the new path refers to a "
                        "different file", 0);
    }
    if(restoreOffset){
        os::lseek(newFd, os::ltell(oldFd), SEEK_SET);
    }
    closeOnErr.setEnabled(false);
    return newFd;
}



std::string osutil::parseGenericKeyValFile(int dirFd,
                                           const std::string &filename,
                                           const std::string &key)
{
    int fd = os::openat(dirFd, filename.c_str(), O_RDONLY);
    // closes fd in destrcutor
    __gnu_cxx::stdio_filebuf<char> filebuf(fd, std::ios::in);
    std::istream is(&filebuf);

    std::string line;
    while(getline(is, line)){
        std::string currentKey;
        std::stringstream wordStream(line);
        if( !(wordStream >> currentKey)){
             continue;
        }
        if(currentKey != key){
            continue;
        }
        std::string val;
        if( wordStream >> val){
            return val;
        } 
        break;
        
    }
    return std::string();

}



std::string osutil::fcntlflagsToString(int flags)
{
    std::string o;
    if (flags & O_WRONLY){
        o += "O_WRONLY ";
    } else if (flags & O_RDWR){
        o += "O_RDWR ";
    } else {
        o += "O_RDONLY ";
    }

    if (flags & O_CREAT)
        o += "O_CREAT ";
    if (flags & O_CLOEXEC)
        o += "O_CLOEXEC ";
    if (flags & O_DIRECTORY)
        o += "O_DIRECTORY ";
    if (flags & O_EXCL)
        o += "O_EXCL ";
    if (flags & O_NOCTTY)
        o += "O_NOCTTY ";
    if (flags & O_NOFOLLOW)
        o += "O_NOFOLLOW ";
#ifdef O_TMPFILE
    if (flags & O_TMPFILE)
        o += "O_TMPFILE ";
#endif

    if (flags & O_APPEND)
        o += "O_APPEND ";
    if (flags & O_ASYNC)
        o += "O_ASYNC ";
    if (flags & O_DIRECT)
        o += "O_DIRECT ";
    if (flags & O_DSYNC)
        o += "O_DSYNC ";
    if (flags & O_LARGEFILE)
        o += "O_LARGEFILE ";
    if (flags & O_NOATIME)
        o += "O_NOATIME ";
    if (flags & O_NONBLOCK)
        o += "O_NONBLOCK ";
    if (flags & O_PATH)
        o += "O_PATH ";
    if (flags & O_DSYNC)
        o += "O_DSYNC ";
    if (flags & O_SYNC)
        o += "O_SYNC ";
    if (flags & O_TRUNC)
        o += "O_TRUNC ";

    return o;
}


/// Merely a debug function
void osutil::printOpenFds(bool onlyRegular)
{
    QIErr() << "open fds:\n";
    for(const int fd : osutil::FdEntries()){
        auto st = os::fstat(fd);
        if( onlyRegular && ! S_ISREG(st.st_mode)){
            continue;
        }
        QByteArray fdPath = QByteArray("/proc/self/fd/") + QByteArray::number(fd);
        auto resolvedPath = os::readlink<QByteArray>(fdPath);
        QIErr() << fd << ": " << resolvedPath;
    }
}

void osutil::randomSleep(int msMin, int msMax)
{
    std::mt19937_64 eng{std::random_device{}()};
    std::uniform_int_distribution<> dist{msMin, msMax};
    std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
}

/// For most efficient usage assign a bufsize a little larger than the (probable)
/// file size.
QByteArray osutil::readWholeFile(int fd, int bufSize)
{
    assert(bufSize > 0);
    QByteArray buf;
    buf.resize(bufSize);
    int offset=0;
    while(true){
        char* dataPtr = buf.data() + offset;
        auto readCount = os::read(fd, dataPtr, static_cast<size_t>(bufSize), true);
        if(readCount < bufSize){
            // EOF
            buf.resize(offset + static_cast<int>(readCount));
            return buf;
        }
        offset += bufSize;
        buf.resize(buf.size() + bufSize);
    }

}

/// @param fd: typically STDOUT_FILENO or similar
bool osutil::isTTYForegoundProcess(int fd)
{
    return getpgrp() == tcgetpgrp(fd);
}


/// Wait until a typical 'TERM'-signal occurs. During wait the signal handlers
/// are overridden and restored afterwards.
void osutil::waitForSignals()
{
    QVarLengthArray<sighandler_t, 64> oldHandlers;

    // wait for typical signals to exit
    for(int s : os::catchableTermSignals()){
        oldHandlers.push_back(os::signal(s, [](int){}));
    }
    sigset_t sigs;
    os::sigfillset(&sigs);
    os::sigwait(&sigs);
    for (int i=0; i < oldHandlers.size(); ++i) {
        os::signal(os::catchableTermSignals()[static_cast<size_t>(i)], oldHandlers[i]);
    }

}

/// Set a signal handler doing nothing for the specified signales.
/// Note that this is *not* equivalent to SIG_IGN:
/// SIG_IGN is inherited on execve, our signal handler is not.
void osutil::setInertSighandler(const std::vector<int> &sigs)
{
    struct sigaction act{};
    act.sa_handler = intertSighandler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;
    for(auto s : sigs){
        os::sigaction(s, &act, nullptr);
    }
}


/// be verbose in case os::close fails
void osutil::closeVerbose(int fd)
{
    try {
        os::close(fd);
    } catch (const os::ExcOs& e) {
        std::cerr << e.what()
                  << generate_trace_string()
                  << "\n";
    }
}

/// Filedescriptors are usually given out using low integers, this function allows
/// for finding the highest fd starting at startFd. If startFd==-1, return the
/// the highest possible free fd (per-process max.-fd-count is e.g. 1024).
/// @return: A fd-number if a free fd could be found. The fd is not opened, so there is
/// a race-condition here. If no fd in the given range could be found, return -1.
int osutil::findHighestFreeFd(int startFd, int minFd){
    int fd = (startFd == -1) ? static_cast<int>(osutil::getMaxCountOpenFiles() -1)
                             : startFd;
    for(; fd >= minFd; --fd) {
        if(fd == 255 ){ // that one is usually reserved
            continue;
        }
        if(! osutil::fdIsOpen(fd)){
            return fd;
        }
    }
    return -1;
}

/// @param path: if empty, it will be filled with tmp.xxx at system's tempdir
int osutil::mktmp(QByteArray &path, int flags){
    if(path.isEmpty()){
        path = QDir::tempPath().toUtf8();
        path = pathJoinFilename(path, QByteArray("tmp.XXXXXX"));
    }
    int fd = ::mkostemp(path.data(), flags);
    if (fd < 0) {
        throw os::ExcOs("osutil::mktmp failed");
    }
    return fd;
}

int osutil::mktmp(int flags){
    QByteArray p(QDir::tempPath().toUtf8());
    p = pathJoinFilename(p, QByteArray("tmp.XXXXXX"));
    return osutil::mktmp(p, flags);
}

int osutil::unnamed_tmp(int flags){
    // tmpfs and possibly other filesystems do not suppot O_TMPFILE, for
    // the sake of simplicity just use mkostemp.
#if false
    //#ifdef O_TMPFILE
    int fd = os::open(p, O_RDWR | O_TMPFILE | O_EXCL | o_flags, true, S_IRUSR | S_IWUSR);
#else
    QByteArray p;
    int fd = osutil::mktmp(p, flags);
    if(remove(p) < 0){
        fprintf(stderr, "%s: failed to delete the just created file %s - %s\n",
                __func__, p.constData(), strerror(errno));
    }
#endif
    return fd;
}

