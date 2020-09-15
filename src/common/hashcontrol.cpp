
#include "hashcontrol.h"

/// xxhash parts of a file (or the whole file in case of a small one) according to the
/// specified hashmeta-parameters.
/// @return hash-value of null, if 0 bytes were read.
/// @throws ExcOs, CXXHashError
HashValue HashControl::genPartlyHash(int fd, qint64 filesize, const HashMeta &hashMeta,
                                     bool resetOffset)
{
    const off64_t seektstep = filesize / hashMeta.maxCountOfReads;
    CXXHash::DigestResult hashRes = m_hash.digestFile(
                        fd,
                        hashMeta.chunkSize,
                        seektstep ,
                        hashMeta.maxCountOfReads);
    HashValue hashVal;
    if(hashRes.countOfbytes > 0){
        if(resetOffset){
            os::lseek(fd, 0, SEEK_SET);
        }
        hashVal = hashRes.hash;
    }
    return hashVal;
}


CXXHash &HashControl::getXXHash()
{
    return m_hash;
}

