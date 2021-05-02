#include <sys/file.h>
#include <iostream>

#include "cflock.h"
#include "os.h"

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
    os::flock(m_fd, LOCK_EX);
    m_isLocked = true;
}

void CFlock::lockShared()
{
    os::flock(m_fd, LOCK_SH);
    m_isLocked = true;
}

void CFlock::unlock()
{
    os::flock(m_fd, LOCK_UN);
    m_isLocked = false;
}
