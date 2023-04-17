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
    m_fd(fd),
    m_isLocked(false)
{}

CFlock::~CFlock()
{
    if(m_isLocked){
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
    os::flock(m_fd, LOCK_EX);
    m_isLocked = true;
}

void CFlock::lockShared()
{
    assert(checkFdFlockFlags(m_fd, LOCK_SH));
    os::flock(m_fd, LOCK_SH);
    m_isLocked = true;
}

void CFlock::unlock()
{
    os::flock(m_fd, LOCK_UN);
    m_isLocked = false;
}
