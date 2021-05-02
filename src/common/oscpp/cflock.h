#pragma once


/// Warpper class for flock
class CFlock
{
public:
    CFlock(int fd);
    ~CFlock();

    void lockExclusive();
    void lockShared();
    void unlock();



public:
    CFlock(const CFlock &) = delete ;
    void operator=(const CFlock &) = delete ;

private:
    int m_fd;
    bool m_isLocked;
};

