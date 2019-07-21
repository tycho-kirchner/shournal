#pragma once

#include <qglobal.h>
#include <stddef.h>

struct HashMeta
{
    typedef int size_type;

    HashMeta();
    HashMeta(size_type chunks, size_type maxCountOfR);
    size_type chunkSize;
    size_type maxCountOfReads;

    bool isNull() const;

    bool operator==(const HashMeta& rhs) const;
};

