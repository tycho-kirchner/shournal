
#include <cassert>

#include "cxxhash.h"
#include <iostream>

#include "excos.h"
#include "os.h"


CXXHash::CXXHash() :
    m_pXXState(XXH64_createState())
{
    assert(m_pXXState != nullptr);
    m_buf.resize(sysconf(_SC_PAGESIZE));
}

CXXHash::~CXXHash()
{
    XXH64_freeState(m_pXXState);
}

void CXXHash::resizeBuf(size_t n)
{
    assert(n > 0);
    m_buf.resize(n);
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

struct partial_xxhash_result CXXHash::digestWholeFile(int fd, int chunksize)
{
    return this->digestFile(fd, chunksize, 0, std::numeric_limits<int>::max());
}

partial_xxhash_result CXXHash::digestFile(int fd, int chunksize,
                                off64_t seekstep, int maxCountOfReads)
{
    struct partial_xxhash part_hash;
    part_hash.xxh_state = m_pXXState;
    part_hash.chunksize = chunksize;
    part_hash.seekstep = seekstep;
    part_hash.max_count_of_reads = maxCountOfReads;
    part_hash.buf = m_buf.data();
    part_hash.bufsize = m_buf.size();

    struct partial_xxhash_result res;
    auto ret = partial_xxh_digest_file(fd, &part_hash, &res);
    if(ret != 0){
        throw ExcCXXHash("digest failed: ", int(ret));
    }
    return res;
}


/*
/// XXHASH-digest a whole file or parts of it at regular intervals.
/// @param fd the fildescriptor of the file. Note that in general you would want
///           to make sure, that the offset is at 0. Note that the offset
///           may be changed during the call.
/// @param chunksize size of the chunks to read at once.
/// @param seekstep Read chunks from the file every seekstep bytes. The read chunk
///                 does not count into this, so if you actually want to skip bytes,
///                 seekstep must be greater than chunksize. Otherwise NO SEEK is
///                 performed at all.
/// @param maxCountOfReads stop reading and digest after that count of 'read'-
///                        operations.
/// @returns the calculated hash and the actual count of bytes read.
///          If the count of bytes is zero, the hash is invalid.
/// @throws ExcOs, CXXHashError
CXXHash::DigestResult CXXHash::digestFile(int fd, int chunksize,
                                off64_t seekstep, int maxCountOfReads)
{
    /// Implementation detail:
    /// Calling XXH64_update introduces some overhead, which can be avoided by
    /// calling XXH64() directly with a sufficiently large buffer.
    /// So, if our buffer is large enough, read the chunks from file
    /// one by one into our own buffer. If it's full, call XXH64_update,
    /// else do it alltogether at the end.

    assert(maxCountOfReads > 0);
    assert(chunksize > 0);
    if(chunksize > int(m_buf.size())){
        m_buf.resize(chunksize);
    }
    const bool doSeek = seekstep > chunksize;

    DigestResult res;
    off64_t offset=0;
    res.countOfbytes = 0;
    char* bufRaw = m_buf.data();
    char* bufRawEnd = bufRaw + m_buf.size();
    bool updateNecessary = false;
    for(int countOfReads=0; countOfReads < maxCountOfReads ; ++countOfReads) {
        ssize_t readBytes = os::read(fd, bufRaw, static_cast<size_t>(chunksize));
        bufRaw += readBytes;
        res.countOfbytes += readBytes;
        if(readBytes < chunksize) {
            break; // EOF
        }
        if(bufRawEnd - bufRaw <= chunksize){
            // not enough space for another read: flush buffer
            if(! updateNecessary){
                updateNecessary = true;
                this->reset(0);
            }
            this->update(m_buf.data(), bufRaw - m_buf.data());
            bufRaw = m_buf.data();
        }
        if( doSeek ) {
            offset += seekstep;
            os::lseek(fd, offset, SEEK_SET);
        }
    }
    if(res.countOfbytes == 0){
        res.hash = 0;
        return res;
    }
    // we read something
    if(updateNecessary){
        if(bufRaw != m_buf.data()){
            this->update(m_buf.data(), bufRaw - m_buf.data());
        }
        res.hash = XXH64_digest(m_pXXState);
        return res;
    }
    // No update was needed (all chunks fitted into buffer). Flush the whole buffer at once without
    // xxhash state overhead (update/reset)
    assert(bufRaw != m_buf.data());
    res.hash = XXH64(m_buf.data(), bufRaw - m_buf.data(), 0 );
    return res;
}
*/


CXXHash::ExcCXXHash::ExcCXXHash(const std::string &msg, int errorcode) :
    m_errorcode(errorcode) {
    m_descrip = "XXHashError occurred: " +
            msg + " - errorcode: " +
            std::to_string(m_errorcode) ;
}


const char *CXXHash::ExcCXXHash::what() const noexcept {
    return m_descrip.c_str();
}
