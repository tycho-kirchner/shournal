
#include <string>
#include <iostream>

#include "fdentries.h"
#include "os.h"


osutil::FdEntries::Iterator::Iterator(DIR *dir, int dirfd):
    m_iter_dir(dir),
    m_iter_fd(-1),
    m_iter_dirfd(dirfd)
{
    if(dir != nullptr){
        this->operator++();
    } // else we are the end-iterator

}

osutil::FdEntries::Iterator osutil::FdEntries::Iterator::operator++() {
    struct dirent *ent;
    while ((ent = ::readdir (m_iter_dir)) != nullptr) {
        if(ent->d_name[0] == '.'){
            continue;
        }
        int fd = std::stoi(ent->d_name);
        if(fd == m_iter_dirfd){
            continue;
        }
        m_iter_fd = fd;
        return *this;
    }
    m_iter_fd = -1;
    return *this;
}

bool osutil::FdEntries::Iterator::operator!=(const FdEntries::Iterator &other) const {
    return m_iter_fd != other.m_iter_fd;
}

int osutil::FdEntries::Iterator::operator*() const {
    return m_iter_fd;
}


osutil::FdEntries::FdEntries()
{
    m_dir =  ::opendir ("/proc/self/fd");
    if (m_dir == nullptr) {
        throw os::ExcOs("opendir failed: /proc/self/fd ");
    }
    m_dirLoc = telldir(m_dir);
    if(m_dirLoc == -1){
        throw os::ExcOs("telldir failed");
    }
}

osutil::FdEntries::~FdEntries()
{
    if(closedir (m_dir) == -1){
        std::cerr << __func__ << " closedir failed: " << strerror(errno)
                  << "(" << errno <<")\n";
    }
}

osutil::FdEntries::Iterator osutil::FdEntries::begin() const {
    ::seekdir(m_dir, m_dirLoc);
    return {m_dir, dirfd(m_dir)};
}

osutil::FdEntries::Iterator osutil::FdEntries::end() const {
    return {nullptr, -1};
}




