#pragma once


#include <cstddef>
#include <unistd.h>
#include <string>
#include <limits>

#include "xxhash.h"
#include "strlight.h"
#include "xxhash_common.h"


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
        const char *what () const noexcept override;

    private:
        std::string m_descrip;
        int m_errorcode;
    };


    CXXHash();
    ~CXXHash();

    void resizeBuf(size_t n);

    struct partial_xxhash_result digestWholeFile(int fd, int chunksize);
    struct partial_xxhash_result digestFile(int fd, int chunksize, off64_t seekstep,
                            int maxCountOfReads=std::numeric_limits<int>::max());

public:
    CXXHash(const CXXHash&) = delete;
    void operator=(const CXXHash&) = delete;

private:
    void reset(unsigned long long seed=0);
    void update(const void* buffer, size_t len);

    XXH64_state_t * const m_pXXState;
    StrLight m_buf;
};




