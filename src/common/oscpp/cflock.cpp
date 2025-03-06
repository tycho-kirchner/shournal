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
    if(m_isLockedSH){
        throw QExcProgramming("Due to NFS issues, upgrading shared to exclusive "
                              "locks is not supported. Please unlock() first.");
    }
    os::flock(m_fd, LOCK_EX);
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
