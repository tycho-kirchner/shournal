#pragma once

#include "nullable_value.h"
#include "cxxhash.h"
#include "hashmeta.h"
#include "os.h"

class HashControl
{
public:

    HashValue genPartlyHash(int fd, qint64 filesize, const HashMeta& hashMeta,
                            bool resetOffset=true);
    CXXHash& getXXHash();
private:
    CXXHash m_hash;
};



