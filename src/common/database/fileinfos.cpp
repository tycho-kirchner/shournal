#include "fileinfos.h"

#include "util.h"
#include "db_conversions.h"


bool
FileWriteInfo::operator==(const FileWriteInfo &rhs) const
{
    return mtime == rhs.mtime &&
            size == rhs.size &&
            path == rhs.path &&
            name == rhs.name &&
            hash == rhs.hash;
}

bool
FileReadInfo::operator==(const FileReadInfo &rhs) const
{
    if(idInDb != db::INVALID_INT_ID && rhs.idInDb != db::INVALID_INT_ID){
        return idInDb == rhs.idInDb;
    }

    return mtime == rhs.mtime &&
            size == rhs.size &&
            path == rhs.path &&
            name == rhs.name &&
            mode == rhs.mode &&
            hash == rhs.hash;
}
