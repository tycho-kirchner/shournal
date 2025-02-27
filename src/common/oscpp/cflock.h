#pragma once


/// Wrapper class for flock.
/// Due to NFS emulating flock as fcntl(2) byte-range locks
/// the fd open mode must match the locking operations:
/// In order to place a shared lock, fd must be open for reading,
/// In order to place an exclusive lock, fd must be open for writing. To
/// place both types of lock, open a file read-write.
class CFlock
{
public:
    /// fd should in general be opened read-write (see above)!
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
    bool m_isLockedSH{false};
    bool m_isLockedEX{false};
};

