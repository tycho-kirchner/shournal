
#include <cassert>

#include "cxxhash.h"
#include <iostream>

#include "excos.h"
#include "os.h"


CXXHash::CXXHash() :
    m_pXXState(XXH64_createState())
{
    assert(m_pXXState != nullptr);
}

CXXHash::~CXXHash()
{
    XXH64_freeState(m_pXXState);
}

/// @throws CXXHashError
void CXXHash::reset(unsigned long long seed)
{
    XXH_errorcode err=XXH64_reset(m_pXXState, seed);
    if(err == XXH_ERROR ){
        throw ExcCXXHash("reset failed", err);
    }
}

/// @throws CXXHashError
void CXXHash::update(const void *buffer, size_t len)
{
    XXH_errorcode err = XXH64_update(m_pXXState, buffer, len);
    if(err == XXH_ERROR ){
        throw ExcCXXHash("update failed", err);
    }
}

CXXHash::DigestResult CXXHash::digestWholeFile(int fd, int bufSize)
{
    return this->digestFile(fd, bufSize, 0, std::numeric_limits<int>::max());
}


/// XXHASH-digest a whole file or parts of it at regular intervals.
/// @param fd the fildescriptor of the file. Note that in general you would want
///           to make sure, that the seed is at 0. After the function call the seed
///           will be near EOF.
/// @param bufSize size of the chunks read at once.
/// @param seekstep count of bytes to be added to seed BEFORE the file was read.
///                 This also means that if you actually want to skip bytes,
///                 seekstep must be greater than bufSize. Otherwise NO SEEK is
///                 performed at all.
/// @param maxCountOfReads stop reading and digest after that count of 'read'-
///                        operations.
/// @returns the calculated hash, the count of bytes read and the actual count
///          of reads which is never greater than param maxCountOfReads.
///          If the actual count of reads is zero, the hash is invalid.
/// @throws ExcOs, CXXHashError
CXXHash::DigestResult CXXHash::digestFile(int fd, int bufSize,
                                off64_t seekstep, int maxCountOfReads)
{
    m_buf.resize(bufSize);
    this->reset(0);
    const bool doSeek = seekstep > bufSize;

    DigestResult res {0, 0, 0};
    off64_t offset=0;

    for(res.countOfReads=0; res.countOfReads < maxCountOfReads ; ++res.countOfReads) {
        ssize_t readBytes = os::read(fd, m_buf.data(), static_cast<size_t>(m_buf.size()));
        if(readBytes == 0) {
            break; // EOF
        }
        res.countOfbytes += readBytes;

        this->update(m_buf.data(), static_cast<size_t>(readBytes));

        if( doSeek  ) {
            offset += seekstep;
            os::lseek(fd, offset, SEEK_SET);
        }
    }

    if(res.countOfReads > 0) {
        res.hash = XXH64_digest(m_pXXState);
    }
    return res;
}


CXXHash::ExcCXXHash::ExcCXXHash(const std::string &msg, int errorcode) :
    m_errorcode(errorcode) {
    m_descrip = "XXHashError occurred: " +
            msg + " - errorcode: " +
            std::to_string(m_errorcode) ;
}


const char *CXXHash::ExcCXXHash::what() const noexcept {
    return m_descrip.c_str();
}
