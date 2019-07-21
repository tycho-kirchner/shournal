#pragma once


#include <stddef.h>
#include <unistd.h>
#include <QByteArray>
#include <string>

#include "xxhash.h"

/// A cpp interface around the needed c-functions of XXHASH and
/// some other methods (digestFile).
/// For further documentation of the wrapper-only-functions please head to the
/// documentation of the c-api.
class CXXHash
{
public:
    class ExcCXXHash : public std::exception
    {
    public:
        explicit ExcCXXHash(const std::string & msg, int errorcode);
        const char *what () const noexcept;
    private:
        std::string m_descrip;
        int m_errorcode;
    };

    struct DigestResult {
        XXH64_hash_t hash;
        int countOfReads;
        off64_t countOfbytes;   // number of read bytes
    };

    CXXHash();
    ~CXXHash();

    void reset(unsigned long long seed=0);
    void update(const void* buffer, size_t len);

    DigestResult digestWholeFile(int fd, int bufSize);
    DigestResult digestFile(int fd, int bufSize, off64_t seekstep,
                            int maxCountOfReads=std::numeric_limits<int>::max());

private:
    XXH64_state_t * const m_pXXState;
    QByteArray m_buf;
};

