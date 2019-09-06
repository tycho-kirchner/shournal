#include "hashmeta.h"


HashMeta::HashMeta(size_type chunks, size_type maxCountOfR)
    : chunkSize(chunks),
      maxCountOfReads(maxCountOfR)
{}

bool HashMeta::isNull() const
{
    return chunkSize == 0 && maxCountOfReads ==0;
}

bool HashMeta::operator==(const HashMeta &rhs) const
{
    return chunkSize == rhs.chunkSize &&
           maxCountOfReads == rhs.maxCountOfReads;
}
