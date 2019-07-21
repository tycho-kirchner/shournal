#pragma once

#include <dirent.h>

namespace osutil {


/// Allow iterating for entries in /proc/self/fd.
/// The dir-fd internally used is skipped.
class FdEntries
{
public:
    class Iterator {
        friend class FdEntries;
    public:
        Iterator operator++();
        bool operator!=(const Iterator & other) const;
        int operator*() const;
    private:
        Iterator(DIR * dir, int dirfd);

        DIR* m_iter_dir;
        int m_iter_fd;
        int m_iter_dirfd; // fd of *our* DIR stream
    };

public:
    FdEntries();
    ~FdEntries();

    Iterator begin() const;
    Iterator end() const;

public:
    FdEntries(const FdEntries &) = delete ;
    void operator=(const FdEntries &) = delete ;

private:

    DIR* m_dir;
    long m_dirLoc;
};


}
