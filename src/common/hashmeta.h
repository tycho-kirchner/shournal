#pragma once

#include <qglobal.h>
#include <cstddef>

#include "database/db_globals.h"

struct HashMeta
{
    typedef int size_type;

    HashMeta() = default;
    HashMeta(size_type chunks, size_type maxCountOfR);
    size_type chunkSize {};
    size_type maxCountOfReads {};
    qint64 idInDb {db::INVALID_INT_ID} ;

    bool isNull() const;

    bool operator==(const HashMeta& rhs) const;
};

