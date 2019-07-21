


#include <sys/mount.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <fcntl.h>
#include <climits>
#include <dirent.h>
#include <cstring>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/sendfile.h>

#include <cstdio>

#include <algorithm>
#include <sstream>
#include <iostream>
#include <iterator>
#include <dlfcn.h>

#include "os.h"
#include "excos.h"
#include "cleanupresource.h"
#include "osutil.h"


static bool& retryOnInterrupt(){
    thread_local static bool retryIt = false;
    return retryIt;
}


void os::setRetryOnInterrupt(bool val)
{
    bool & valRef = retryOnInterrupt();
    valRef = val;
}

/// @throws ExcOs
os::stat_t os::fstat(int fd)
{
    struct stat stat_;
    if(::fstat(fd, &stat_) == -1){
        throw ExcOs("fstat " + std::to_string(fd) + " failed");
    }
    return stat_;
}

/// @throws ExcOs
os::stat_t os::stat(const std::string &filename)
{
    int fd = os::open(filename, O_RDONLY);
    try {
        auto stat = os::fstat(fd);
        return stat;
    } catch(...){
        ::close(fd);
        throw ;
    }
}



/// @throws ExcOs
void os::getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{

    if(::getresgid(rgid, egid, sgid) == -1) {
         throw ExcOs("getresgid failed");
    }

}

/// @throws ExcOs
void os::getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    if(::getresuid(ruid, euid, suid) == -1) {
         throw ExcOs("getresuid failed");
    }
}

/// @throws ExcOs
void os::setgid(gid_t gid)
{
    if(::setgid(gid) == -1){
        throw ExcOs("setgid failed");
    }

}

/// @throws ExcOs
void os::setuid(uid_t uid)
{
    if(::setuid(uid) == -1){
        throw ExcOs("setuid failed");
    }
}

std::string os::getHomeDir()
{
    char *homedir = getenv("HOME");
    // fallback
    if (homedir == nullptr) {
        homedir = ::getpwuid(getuid())->pw_dir;
    }
    return homedir;
}


std::string os::getCacheDir()
{
    char *cacheDir = getenv("XDG_CACHE_HOME");
    std::string cacheDirStr;
    // fallback
    if (cacheDir == nullptr) {
        cacheDirStr = os::getHomeDir() + "/.cache";
    } else {
        cacheDirStr = cacheDir;
    }
    return cacheDirStr;
}

/// @throws ExcOs
void os::rename(const std::string &old, const std::string &new_)
{
    if(::rename(old.c_str(), new_.c_str()) == -1){
        throw ExcOs("rename failed");
    }
}

/// @throws ExcOs
void os::close(int fd)
{
    if(::close(fd) == -1){
        throw ExcOs("close failed for fd " + std::to_string(fd));
    }
}


/// @throws ExcOs
int os::open(const char*  filename, int flags, bool clo_exec, mode_t mode){
    if(clo_exec){
        flags |= O_CLOEXEC;
    }
    // quoting the man:
    // the mode argument must be supplied when
    // O_CREAT or O_TMPFILE is specified in flags; if neither O_CREAT
    // nor O_TMPFILE is specified, then mode is ignored.
    // So we always pass the mode.
    int fd = ::open(filename, flags, mode);
    if(fd == -1) {
        throw ExcOs("open " + std::string(filename) + " failed");
    }
    return fd;
}



/// @throws ExcOs
ssize_t os::read(int fd, void *buf, size_t nbytes, bool retryOnInterrupt)
{
    while (true) {
        auto read = ::read(fd, buf, nbytes);
        if(read == -1){
            if(retryOnInterrupt && errno == EINTR){
                continue;
            }
            throw ExcOs("read failed");
        }
        return read;
    }
}



/// @param throwIfLessBytesWritten: if true, throw if the number of written bytes
/// is less than requested (in param n)
/// @throws ExcOs, ExcTooFewBytesWritten
ssize_t os::write(int fd, const void *buf, size_t n, bool throwIfLessBytesWritten)
{
    auto ret = ::write(fd, buf, n);
    if(ret == -1){
        throw ExcOs("write failed");
    }
    if(throwIfLessBytesWritten && ret < static_cast<ssize_t>(n)){
        throw ExcTooFewBytesWritten("Too few bytes written for file " +
                                    osutil::findPathOfFd<std::string>(fd), 0);
    }
    return ret;
}

/// @overload
/// @throws ExcOs
ssize_t os::write(int fd, const std::string &buf, bool throwIfLessBytesWritten)
{
    return os::write(fd, buf.c_str(), buf.size(),
                     throwIfLessBytesWritten);
}

/// @overload
/// @throws ExcOs
ssize_t os::write(int fd, const QByteArray &buf, bool throwIfLessBytesWritten)
{
    return os::write(fd, buf.data(), static_cast<size_t>(buf.size()),
                     throwIfLessBytesWritten);
}



/// Return a pipe-array, where idx 0 holds read- idx 1
/// holds the write end
/// @throws ExcOs
os::Pipes_t os::pipe(int flags, bool clo_exec)
{
    if(clo_exec){
        flags |= O_CLOEXEC;
    }

    Pipes_t fds;
    if(::pipe2(fds.data(), flags) == -1){
        throw ExcOs("pipe failed, used flags: " + std::to_string(flags));
    }
    return fds;
}

/// @throws ExcOs
pid_t os::fork()
{
    auto pid = ::fork();
    if(pid == -1){
        throw ExcOs("fork failed");
    }
    return pid;
}


/// @throws ExcOs
void os::umount(const std::string &specialFile)
{
    int res = ::umount(specialFile.c_str());
    if(res == -1){
        throw ExcOs("umount failed");
    }
}


/// @throws ExcOs
void os::setpriority(int which, id_t who, int prio)
{
    if(::setpriority(which, who, prio) == -1){
         throw ExcOs("setpriority failed");
    }
}


/// @throws ExcOs
void os::setegid(gid_t gid)
{
    if(::setegid(gid) == -1){
        throw ExcOs("setegid failed");
    }
}

/// @throws ExcOs
void os::seteuid(uid_t uid)
{
    if(::seteuid(uid) == -1){
        throw ExcOs("seteuid failed");
    }

}

uid_t os::getuid()
{
    return ::getuid();
}

gid_t os::getgid()
{
    return ::getgid();
}

/// @throws ExcOs
uid_t os::getsuid()
{
   uid_t ruid, euid, suid;
   os::getresuid(&ruid, &euid, &suid);
   return suid;
}

/// @throws ExcOs
gid_t os::getsgid()
{
    gid_t rgid, egid, sgid;
    os::getresgid(&rgid, &egid, &sgid);
    return sgid;
}

/// aquivalent of 'mkdir -p' to create necessary
/// parts of a path recursively
void os::mkpath(std::string s, mode_t mode)
{
    size_t pre=0, pos;
    std::string dir;
    int mdret;
    if(s[s.size()-1]!='/'){
        s+='/';
    }

    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.empty()){
            continue;
        }
        if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            throw ExcOs("mkpath failed");
        }
    }
}



int os::openat(int dirfd, const char* filename, int flags, bool clo_exec, mode_t mode)
{
    if(clo_exec){
        flags |= O_CLOEXEC;
    }
    int fd = ::openat(dirfd, filename, flags, mode);
    if(fd == -1){
        throw ExcOs("openat " + std::string(filename) + " failed");
    }
    return fd;
}



/// Be careful, according to man, "It is unspecified whether the effective group
/// ID of the calling process is included in the returned list."
/// @throws ExcOs
os::Groups os::getgroups()
{
    os::Groups groups;
    int ngroups = ::getgroups(0, groups.data());
    if(ngroups == -1){
        throw ExcOs("getgroups");
    }
    groups.resize(static_cast<os::Groups::size_type>(ngroups));
    ngroups = ::getgroups(static_cast<int>(groups.size()), groups.data());
    if(ngroups == -1){
        throw ExcOs("getgroups");
    }
    return groups;
}

uid_t os::geteuid()
{
    return ::geteuid();
}

gid_t os::getegid()
{
    return ::getegid();
}


/// returns a list of all group-ids on the system (/etc/group)
/// @throws ExcOs
std::vector<gid_t> os::queryGroupIds()
{    
    std::vector<gid_t> ids;
    group* grp;
    ::setgrent();

    errno = 0;
    while((grp = getgrent()) != nullptr){
        ids.push_back(grp->gr_gid);
    }
    if(errno != 0){
        throw ExcOs("getgrent");
    }
    endgrent();
    return ids;
}




/// @throws ExcKernelVersionParse, ExcOs
os::KernelVersion os::getKernelVersion()
{
    utsname uname_;
    if(uname(&uname_) == -1){
         throw ExcOs("uname");
    }
    KernelVersion version;
    int verIdx=0;
    std::string release = uname_.release;
    std::string currentNumber;
    for(const char c : release){
        if(std::isdigit(c)){
            currentNumber += c;
        } else{
            version[static_cast<KernelVersion::size_type>(verIdx++)] = std::stoi(currentNumber);
            currentNumber.clear();
            if(verIdx == version.size()){
                break;
            }
        }
    }
    if(verIdx != version.size()){
        throw ExcKernelVersionParse();
    }
    return version;
}

int os::unshare(int flags)
{
    int ret = ::unshare(flags);
    if(ret == -1){
        throw ExcOs("unshare failed");
    }
    return ret;
}

/// returns the names of the directory contents
/// @throws ExcOs
std::vector<std::string>  os::ls(const std::string &dirname_, os::DirFilter filter)
{
    DIR *dir;
    struct dirent *ent;
    if ((dir = ::opendir (dirname_.c_str())) == nullptr) {
        throw ExcOs("opendir failed: " + dirname_);
    }
    auto closeLater = finally([&dir] {  closedir (dir);  });

    std::vector<std::string> files;
    while ((ent = ::readdir (dir)) != nullptr) {
        if((filter & DirFilter::NoDot && strcmp(ent->d_name, "." ) == 0) ||
           (filter & DirFilter::NoDotDot && strcmp(ent->d_name, ".." ) == 0)){
            continue;
        }
        files.emplace_back(ent->d_name);
    }

    return files;
}




pid_t os::getpid()
{
    return ::getpid();
}

/// @param cleanStatusOnSuccess:: If the child terminated normally, clean status, so
/// it only contains the 8 least significant bits (WEXITSTATUS).
/// @throws ExcOs, ExcProcessExitNotNormal
pid_t os::waitpid(pid_t pid, int *status, int options, bool cleanStatusOnSuccess)
{
    int internalStatus = 1;
    if(status == nullptr){
        status = &internalStatus;
    }
    pid_t ret = ::waitpid (pid, status, options) ;
    if(ret == -1){
        throw ExcOs("waitpid failed for pid " + std::to_string(pid) );
    }
    if (! WIFEXITED (*status)){
        // process did not call exit and did not return from main normally
        // find out what happended
        int extractedStatus=-1;
        ExcProcessExitNotNormal::TypeOfTerm typeOfTerm = ExcProcessExitNotNormal::NOT_IMPLEMENTED;
        if(WIFSIGNALED(*status)){
            extractedStatus = WTERMSIG(*status);
            if(WCOREDUMP(*status)){
                typeOfTerm = ExcProcessExitNotNormal::COREDUMP;
            } else {
                typeOfTerm = ExcProcessExitNotNormal::SIG;
            }
        }
        // There are some other cases, which could be checked:
        // WIFSTOPPED/WIFCONTINUED

        throw ExcProcessExitNotNormal(extractedStatus, typeOfTerm);
    }
    if(cleanStatusOnSuccess){
        *status = WEXITSTATUS(*status);
    }

    return ret;
}

/// @throws ExcOs
void os::exec (const char *filename, char * const argv[], char * const envp[])
{
    if(envp == nullptr){
        envp = environ;
    }
    ::execvpe(filename, argv,  envp);
    // only get here on error
    throw ExcOs("executing " + std::string(filename) + " failed" );

}

/// @throws ExcOs
void os::exec(const std::vector<std::string> &args, char * const envp[])
{
    if (args.empty()) {
        throw std::invalid_argument(
                    "exec called with empty args");
    }

    std::vector<char*> pointerVec(args.size() + 1 ); // + 1 because of terminating NULL
    for(unsigned i = 0; i < args.size() ; ++i) {
        pointerVec[i] = const_cast<char*>(args[i].c_str());
    }
    pointerVec.back() = nullptr;
    char** result = pointerVec.data();
    os::exec(result[0], result, envp);
}




/// @throws ExcOs
void os::setgroups(const os::Groups &groups)
{
    if(::setgroups(groups.size(), groups.data()) == -1){
        std::stringstream result;
        std::copy(groups.begin(), groups.end(), std::ostream_iterator<int>(result, " "));
        throw ExcOs("setgroups failed. Used groups: " + result.str() );
    }
}

/// @throws ExcOs
off_t os::lseek(int fd, off_t offset, int whence)
{
    off_t ret = ::lseek(fd, offset, whence);
    if(ret == -1){
        throw ExcOs("lseek failed. fd: " + std::to_string(fd)
                    + " offset: " + std::to_string(offset));
    }
    return ret;
}

/// Like ftell for a file descriptor
/// @throws ExcOs
off_t os::ltell(int fd)
{
    return os::lseek(fd, 0, SEEK_CUR);

}

void os::mount(const char *source, const char *target, const char *fstype, unsigned long rwflag, const void *data)
{
    if(::mount(source, target, fstype, rwflag, data) == -1){        
        throw ExcOs("Mount from " + strFromCString(source) + " to "
                    + strFromCString(target) + " failed");
    }
}


/// @throws ExcOs
void os::mount(const std::string &source, const std::string& target, const char *fstype,
               unsigned long rwflag, const void *data)
{
    os::mount(source.c_str(), target.c_str(), fstype, rwflag, data);
}

/// @throws ExcOs
void *os::dlsym(void *handle, const char *symbol)
{
    auto sym_ = ::dlsym(handle, symbol);
    if(sym_ == nullptr){
        char* errStr = dlerror();
        if(errStr == nullptr){
            throw ExcOs("dlsym returned null, but dlerror was also null...");
        }
        throw ExcOs("dlsym failed: " + std::string(errStr));
    }
    return sym_;
}


/// @throws ExcOs
void os::setns(int fd, int nstype)
{
    if(::setns(fd, nstype) == -1 ){
        throw ExcOs("Failed to enter namespace " + std::to_string(nstype));
    }
}


pid_t os::setsid()
{
    pid_t sid = ::setsid();
    if(sid == static_cast<pid_t>(- 1) ){
        throw ExcOs("setsid failed");
    }
    return sid;
}



bool os::exists(const std::string &name)
{
      struct stat buffer;
      return (::stat (name.c_str(), &buffer) == 0);
}

/// Shortcut for fcntl(F_SETFD, ...
/// @throws ExcOs
void os::setFdDescriptorFlags(int fd, int flags)
{
    if(::fcntl(fd, F_SETFD, flags) == -1){
        throw ExcOs(std::string(__func__) + " failed for fd "+
                    std::to_string(fd)
                    + " (flags " + std::to_string(flags) + ")"
                    );
    }

}


int os::dup(int oldfd){
    int newfd = ::dup(oldfd);
    if(newfd == -1){
        throw ExcOs(std::string(__func__) + " failed for fd "+
                    std::to_string(oldfd)
                    );
    }
    return newfd;
}

void os::dup2(int oldfd, int newfd)
{
    if(::dup2(oldfd, newfd) == -1){
        throw ExcOs(std::string(__func__) + " failed for fds "+
                    std::to_string(oldfd) + ", " + std::to_string(newfd)
                    );
    }
}

void os::dup3(int oldfd, int newfd, int flags)
{
    if(::dup3(oldfd, newfd, flags) == -1){
        throw ExcOs(std::string(__func__) + " failed for fds "+
                    std::to_string(oldfd) + ", " + std::to_string(newfd)
                    );
    }
}

/// @throws ExcOs
void os::fchdir(int fd)
{
    if(::fchdir(fd) == -1){
        throw ExcOs("fchdir failed");
    }
}


/// @throws ExcOs
void os::fchmod(int fd, mode_t mode)
{
    if(::fchmod(fd, mode) == -1){
        throw ExcOs("fchmod failed");
    }
}

void os::sigaction(int signum, const struct sigaction *act,
                   struct sigaction *oldact)
{
    if(::sigaction(signum, act, oldact) == -1){
        throw ExcOs(std::string(__func__) + "failed");
    }
}


/// @throws ExcOs
/// @return the signal number
int os::sigwait(const sigset_t *set)
{
    int sig;
    int ret = ::sigwait(set, &sig);
    if(ret != 0){
        // not using errno here!
        throw ExcOs("sigwait failed", ret);
    }
    return sig;
}

/// @throws ExcOs
void os::sigfillset(sigset_t *set)
{
    if(::sigfillset(set) != 0){
        throw ExcOs("sigfillset failed");
    }
}

/// Always returns the old handler
/// @throws ExcOs
sighandler_t os::signal(int sig, sighandler_t handler)
{
    auto oldhandler = ::signal(sig, handler);
    if ( oldhandler == SIG_ERR) {
        throw ExcOs("signal failed");
    }
    return oldhandler;
}

/// @throws ExcOs
void os::chdir(const char *path)
{
    if(::chdir(path) == -1){
        throw ExcOs("chdir failed");
    }
}

void os::chdir(const std::string &path)
{
    return os::chdir(path.data());
}

/// shortcut for fcntl(fd , F_GETFL)
/// @throws ExcOs
int os::getFdStatusFlags(int fd)
{
    int statusflags = fcntl(fd , F_GETFL);
    if(statusflags == -1){
        throw ExcOs("failed to get status flags from fd " + std::to_string(fd));
    }
    return statusflags;
}

/// shortcut for fcntl(fd , F_GETFD)
/// @throws ExcOs
int os::getFdDescriptorFlags(int fd)
{
    int statusflags = fcntl(fd , F_GETFD);
    if(statusflags == -1){
        throw ExcOs("failed to get descriptor flags from fd " + std::to_string(fd));
    }
    return statusflags;
}

/// @return the number of bytes send
/// @throws ExcOs
size_t os::sendmsg(int fd, const msghdr *message, int flags)
{
    while (true){
        ssize_t ret = ::sendmsg(fd, message, flags);
        if (ret == -1) {
            if(retryOnInterrupt() && errno == EINTR){
                continue;
            }
            throw ExcOs(std::string(__func__) + " failed");
        }
        return static_cast<size_t>(ret);
    }



}

/// offset of in_fd is *not* modified, startoffset is always 0
/// @return number of sent bytes
off_t os::sendfile(int out_fd, int in_fd, size_t count)
{
    off_t offset=0;
    auto sizeToSend = count;
    while (true) {
        auto sent = ::sendfile(out_fd, in_fd, &offset, sizeToSend);
        if(sent == -1){
            throw ExcOs(std::string(__func__) + " failed");
        }
        if(offset == static_cast<off_t>(count) || sent == 0){
            break;
        }
        sizeToSend -= static_cast<size_t>(sent);
    }
    return offset;
}

/// returns the number of bytes received
/// @throws ExcOs
size_t os::recvmsg(int fd, msghdr *message, int flags)
{
    while (true){
        ssize_t ret = ::recvmsg(fd, message, flags);
        if (ret == -1) {
            if(retryOnInterrupt() && errno == EINTR){
                continue;
            }
            throw ExcOs(std::string(__func__) + " failed");
        }
        return static_cast<size_t>(ret);
    }
}

os::SocketPair_t os::socketpair(int domain, int type_, int protocol)
{
    SocketPair_t pair;
    if(::socketpair(domain, type_, protocol, pair.data()) == -1){
        throw ExcOs(std::string(__func__) + " failed");
    }
    return pair;
}



void os::unsetenv(const char *name)
{
    if(::unsetenv(name) == -1){
        throw ExcOs(std::string(__func__) + " failed");
    }
}


/// Return a rather random array of signals which are catchable and would by default
/// cause a process to end.
const os::CatchableTermSignals& os::catchableTermSignals()
{
    static const CatchableTermSignals sigs {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGPIPE};
    return sigs;
}


void os::flock(int fd, int operation)
{
    if(::flock(fd, operation) == -1){
        throw os::ExcOs("flock failed");
    }

}







