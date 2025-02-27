#include <sys/file.h>
#include <iostream>

#include "cflock.h"
#include "os.h"

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
    try {
        os::flock(m_fd, LOCK_EX);
    } catch (const os::ExcOs& ex) {
        if(ex.errorNumber() != EDEADLK){
            throw;
        }
        // EDEADLK is actually not a true flock error. However, on any non-ancient NFS,
        // flock is implicitly converted to fcntl(2) byte-range locking. On
        // openSUSE Leap 15.6, in scenarios with at least three concurrent lockers
        // converting shared to exclusive locks, EDEADLK occurs frequently. While this
        // might indicate a bug in NFS itself, work it around here anyway, by completely
        // unlocking first.
        bool prevIsLockedSH = m_isLockedSH;
        unlock();
        try {
            os::flock(m_fd, LOCK_EX);
        } catch (const os::ExcOs&) {
            if(prevIsLockedSH){
                std::cerr << "flock(LOCK_EX) failed the second time: " << ex.what()
                          << ". Trying to re-gain the previous shared lock\n";
                lockShared();
            }
            throw;
        }
    }
    m_isLockedSH = false;
    m_isLockedEX = true;
}

void CFlock::lockShared()
{
    assert(checkFdFlockFlags(m_fd, LOCK_SH));
    os::flock(m_fd, LOCK_SH);
    m_isLockedEX = false;
    m_isLockedSH = true;
}

void CFlock::unlock()
{
    os::flock(m_fd, LOCK_UN);
    m_isLockedEX = false;
    m_isLockedSH = false;
}
