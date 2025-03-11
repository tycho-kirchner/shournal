#include <sys/file.h>
#include <iostream>

#include "cflock.h"
#include "os.h"
#include "osutil.h"

#ifndef NDEBUG

static bool checkFdFlockFlags(int fd, int operation){
    int flags = os::getFdStatusFlags(fd);

    switch (operation) {
    case LOCK_EX:
        if(flags & O_WRONLY || flags & O_RDWR) {
            return true;
        }
        std::cerr << "LOCK_EX: fd opened RDONLY\n";
        break;
    case LOCK_SH:
        if(!(flags & O_WRONLY)) {
            return true;
        }
        std::cerr << "LOCK_SH: fd opened WRONLY\n";
        break;
    default:
        std::cerr << "Bad fd operation " << operation << "\n";
        break;
    }
    return false;
}

#endif

/// In order to catch further possible NFS idiosyncrasies (bugs?), better never lock
/// blocking. See also e.g. shournal's commit 1918f88.
static void doLockNB(int fd, int operation){
    for(int i=0; ; i++){
        try {
            os::flock(fd, operation | LOCK_NB);
            return;
        } catch (const os::ExcOs& ex) {
            if(ex.errorNumber() != EWOULDBLOCK){
                throw;
            }
            if(i>9){
                std::cerr << "doLockNB: gave up waiting for lock\n";
                throw;
            }
            osutil::randomSleep(1 *1000, 3 *1000);
        }
    }
}

CFlock::CFlock(int fd) :
    m_fd(fd)
{}

CFlock::~CFlock()
{
    if(m_isLockedSH || m_isLockedEX){
        try {
            unlock();
        } catch (const os::ExcOs& e) {
            // should never happen
            std::cerr << e.what() << "\n";
        }
    }
}

void CFlock::lockExclusive()
{
    assert(checkFdFlockFlags(m_fd, LOCK_EX));
    if(m_isLockedSH){
        throw QExcProgramming("Due to NFS issues, upgrading shared to exclusive "
                              "locks is not supported. Please unlock() first.");
    }
    doLockNB(m_fd, LOCK_EX);
    m_isLockedSH = false;
    m_isLockedEX = true;
}

void CFlock::lockShared()
{
    assert(checkFdFlockFlags(m_fd, LOCK_SH));
    doLockNB(m_fd, LOCK_SH);
    m_isLockedEX = false;
    m_isLockedSH = true;
}

void CFlock::unlock()
{
    os::flock(m_fd, LOCK_UN);
    m_isLockedEX = false;
    m_isLockedSH = false;
}
