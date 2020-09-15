#pragma once


#include <sys/stat.h>
#include <grp.h>

#include <pwd.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string>
#include <array>
#include <vector>
#include <QVector>
#include <deque>
#include <csignal>
#include <QtGlobal>

#include "excos.h"
#include "util.h"


/// Private functions - internal use
namespace __os {

int openat(int dirfd, const char* filename, int flags, bool clo_exec, mode_t mode);

}



/// Simple wrappers for several os calls
/// which throw exceptions on error.
namespace os {

void setRetryOnInterrupt(bool val);

// read/write by owner, read only by group and others
static const int DEFAULT_CREAT_FLAGS = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

extern const int OPEN_WRONLY;
extern const int OPEN_RDONLY;
extern const int OPEN_RDWR;

class ExcKernelVersionParse : public std::exception
{
};

typedef std::array<int, 2> Pipes_t;
typedef std::array<int, 2> SocketPair_t;
typedef std::vector<gid_t> Groups;
typedef std::array<int, 3> KernelVersion; // major minor patch
typedef struct stat stat_t;
//typedef QVarLengthArray<int, 64> CatchableTermSignals;
const int CATCHABLE_TERM_SIGNALS_SIZE = 5;
typedef std::array<int, CATCHABLE_TERM_SIGNALS_SIZE> CatchableTermSignals;


enum DirFilter { NoDot=0x2000, NoDotDot=0x4000, NoDotAndDotDot=NoDot | NoDotDot };

const CatchableTermSignals &catchableTermSignals();

void chdir(const char *path);
void chdir(const std::string& path);
void close(int fd);

void *dlsym (void *handle, const char *symbol);

pid_t fork();

stat_t fstat(int fd);
stat_t stat(const char* filename);

int dup(int oldfd);
void dup2(int oldfd, int newfd);
void dup3(int oldfd, int newfd, int flags);

bool exists(const std::string& name);

void fchdir(int fd);
void fchmod(int fd, mode_t mode);

int getFdStatusFlags(int fd);
int getFdDescriptorFlags(int fd);
std::string getHomeDir();
std::string getCacheDir();

void getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
void getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);

uid_t getuid();
uid_t geteuid();
uid_t getsuid();

gid_t getgid();
gid_t getegid();
gid_t getsgid();

Groups getgroups();

pid_t getpid();
template <class Str_t>
Str_t getUserName();


[[ noreturn ]] void exec(const char *filename, char *const argv[],
                         char *const envp[]=nullptr);
[[ noreturn ]] void exec(const std::vector<std::string> &args,
                         char *const envp[]=nullptr);
template <class ContainerT>
[[ noreturn ]] void exec(const ContainerT &args,
                         char *const envp[]=nullptr);

void flock(int fd, int operation);

std::vector<std::string> ls(const std::string & dirname_,
                            DirFilter filter=DirFilter::NoDotAndDotDot);


off_t lseek (int fd, off_t offset, int whence);
off_t ltell(int fd);

void mkpath(std::string s, mode_t mode=0755);
void mount (const std::string & source, const std::string &target,
          const char *fstype, unsigned long int rwflag,
          const void *data=nullptr);
void mount (const char* source, const char* target,
            const char *fstype, unsigned long int rwflag,
            const void *data=nullptr);


int open(const char*  filename, int flags, bool clo_exec=true,
         mode_t mode=DEFAULT_CREAT_FLAGS);
template <class Str_t>
int open(const Str_t& filename, int flags, bool clo_exec=true,
         mode_t mode=DEFAULT_CREAT_FLAGS);


// int openat(int dirfd, const char *filename, int flags, bool clo_exec=true,
//            mode_t mode=DEFAULT_CREAT_FLAGS);
template <class Str_t>
int openat(int dirfd, const Str_t& filename, int flags,bool clo_exec=true,
           mode_t mode=DEFAULT_CREAT_FLAGS );

int unshare (int flags);

Pipes_t pipe(int flags=0, bool clo_exec=true);

template <class Str_t>
Str_t readlink (const char* filename);
template <class Str_t>
Str_t readlink (const Str_t & filename);

template <class Str_t>
Str_t readlinkat (int dirfd, const Str_t & filename);

void readlinkat (int dirfd, const char* filename, std::string & output);

template <class Str_t>
void readlinkat (int dirfd, const Str_t & filename, Str_t & output);

ssize_t read (int fd, void *buf, size_t nbytes, bool retryOnInterrupt=false);

template <class Str_t>
Str_t readStr(int fd, size_t nbytes, bool retryOnInterrupt=false);

size_t recvmsg (int fd, struct msghdr *message, int flags=0);
template <class Str_t>
void rename(const Str_t & old, const Str_t & new_);

size_t sendmsg (int fd, const struct msghdr *message,
            int flags=0);

off_t sendfile(int out_fd, int in_fd, size_t count);
template <class Str_t>
off_t sendfile(const Str_t& out_path, const Str_t& in_path, size_t count);

void setFdDescriptorFlags(int fd, int flags);

void setgid (gid_t gid);
void setgroups (const Groups & groups);
void setegid(gid_t gid);

void setpriority(int which, id_t who, int prio);

template <class Str_t>
void setenv(const Str_t& name, const Str_t& value,
            bool overwrite=true);

void seteuid(uid_t uid);
void setuid (uid_t uid);

void setns (int fd, int nstype);

pid_t setsid();

void sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact);
int sigwait(const sigset_t *set);
void sigfillset(sigset_t *set);

sighandler_t signal(int sig, sighandler_t handler);

void symlink(const char *target, const char *linkpath);

SocketPair_t socketpair (int domain, int type_, int protocol=0);

void unlinkat(int dirfd, const char *pathname, int flags);

void umount (const std::string& specialFile);
void unsetenv(const char* name);

pid_t waitpid (pid_t pid, int* status=nullptr, int options=0, bool cleanStatusOnSuccess=true);

ssize_t write (int fd, const void *buf, size_t n, bool throwIfLessBytesWritten=true);

ssize_t write (int fd, const std::string &buf, bool throwIfLessBytesWritten=true);
ssize_t write (int fd, const QByteArray &buf, bool throwIfLessBytesWritten=true);


std::vector<gid_t> queryGroupIds();

KernelVersion getKernelVersion();

} // namespace os

template <class ContainerT>
void os::exec(const ContainerT &args, char * const envp[])
{
    if (args.isEmpty()) {
        throw std::invalid_argument(
                    "exec called with empty args");
    }
    os::exec(args[0], (char**)args.data(), envp);
}


template <class Str_t>
Str_t os::readlinkat (int dirfd, const Str_t & filename){
    Str_t path;
    os::readlinkat(dirfd, filename, path);
    return path;
}




template <class Str_t>
void os::readlinkat (int dirfd, const Str_t & filename, Str_t & output){
    output.resize(PATH_MAX);
    char* buf = strDataAccess(output);
    const char* filename_cstr = strDataAccess(filename);
    ssize_t path_len = ::readlinkat(dirfd, filename_cstr, buf, PATH_MAX);
    if (path_len == -1 ){
        throw ExcReadLink("readlinkat failed for file " + std::string(filename_cstr));
    }
    output.resize(static_cast<typename Str_t::size_type>(path_len));
}


/// @throws ExcReadLink
template <class Str_t>
Str_t os::readlink(const char* filename)
{
    Str_t path;
    path.resize(PATH_MAX);
    char* buf = strDataAccess(path);

    ssize_t path_len = ::readlink(filename, buf, PATH_MAX);
    if(path_len == -1){
        throw ExcReadLink("readlink failed for file " + std::string(filename));
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    path.resize(static_cast<typename Str_t::size_type>(path_len));
#else
    // accept the Warning, at least in QT <=5.3 QByteArray has no size_type
    path.resize(path_len);
#endif
    return path;
}


/// @throws ExcReadLink
template <class Str_t>
Str_t os::readlink(const Str_t &filename)
{
    return os::readlink<Str_t>(filename.data());
}


/// @throws ExcOs
template <class Str_t>
int os::open(const Str_t& filename, int flags, bool clo_exec, mode_t mode){
    return os::open(strDataAccess(filename), flags, clo_exec, mode);
}


/// @throws ExcOs
template <class Str_t>
int os::openat(int dirfd, const Str_t& filename, int flags, bool clo_exec, mode_t mode){    
    return __os::openat(dirfd, strDataAccess(filename), flags, clo_exec, mode);
}

/// @return the username of the *real* user
template <class Str_t>
Str_t os::getUserName()
{
    return ::getpwuid(getuid())->pw_name;
}


/// @throws ExcOs
template <class Str_t>
Str_t os::readStr(int fd, size_t nbytes, bool retryOnInterrupt)
{
    Str_t buf;
    buf.resize(nbytes);
    ssize_t readBytes = os::read(fd, buf.data(), buf.size(), retryOnInterrupt);
    buf.resize(static_cast<typename Str_t::size_type>(readBytes));
    return buf;
}


/// @throws ExcOs
template <class Str_t>
void os::rename(const Str_t & old, const Str_t & new_){
    if(::rename(strDataAccess(old), strDataAccess(new_)) == -1){
        throw ExcOs("rename failed");
    }
}

/// @throws ExcOs
template <class Str_t>
void os::setenv(const Str_t &name, const Str_t &value, bool overwrite)
{
    // in contrast to putenv, setenv makes copies of the passed
    // string, so we are safe, when value-string goes out of scope
    if(::setenv(strDataAccess(name), strDataAccess(value), int(overwrite)) == -1){
         throw ExcOs("setenv failed");
    }
}


template <class Str_t>
off_t os::sendfile(const Str_t& out_path, const Str_t& in_path, size_t count){
    int out_fd=-1, in_fd=-1;
    try {
        int out_fd = os::open<Str_t>(out_path, os::OPEN_WRONLY);
        int in_fd = os::open<Str_t>(in_path, os::OPEN_RDONLY);
        auto ret = os::sendfile(out_fd, in_fd, count);
        close(out_fd);
        close(in_fd);
        return ret;
    } catch (const os::ExcOs&) {
        if(out_fd != -1) close(out_fd);
        if(in_fd != -1)  close(in_fd);
        throw ;
    }
}




