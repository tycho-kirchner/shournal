#pragma once

#include <sys/types.h>
#include <string>

template <typename T>
class S_IdMapEntry
{
public:
    T idInNs;
    T idOutOfNs;
    T count;

    S_IdMapEntry(T inNs_, T idOutOfNs_,
                 T count_=1) :
        idInNs(inNs_),
        idOutOfNs(idOutOfNs_),
        count(count_){}

    S_IdMapEntry(T idInBoth) :
        S_IdMapEntry(idInBoth, idInBoth){}

    /*
    S_IdMapEntry(const S_IdMapEntry& other) :
        idInNs(other.idInNs),
        idOutOfNs(other.idOutOfNs),
        count(other.count) {} */

    /// @return the string in the form the gid or uid map expects:
    /// 0 1000 1 (including trailing newline)
    std::string to_string() const
    {
        return std::to_string(idInNs) + " "
                + std::to_string(idOutOfNs) + " "
                + std::to_string(count) + '\n';
    }
};

