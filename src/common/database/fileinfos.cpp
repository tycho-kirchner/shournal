#include "fileinfos.h"
#include "db_globals.h"

FileWriteInfo::FileWriteInfo() :
    size(0)
{

}

bool FileWriteInfo::operator==(const FileWriteInfo &rhs) const
{
    return mtime == rhs.mtime &&
            size == rhs.size &&
            path == rhs.path &&
            name == rhs.name &&
            hash == rhs.hash;
}



FileReadInfo::FileReadInfo() :
    idInDb(db::INVALID_INT_ID),
    size(0),
    mode(0)
{}


bool FileReadInfo::operator==(const FileReadInfo &rhs) const
{
    if(idInDb != db::INVALID_INT_ID && rhs.idInDb != db::INVALID_INT_ID){
        return idInDb == rhs.idInDb;
    }

    return mtime == rhs.mtime &&
            size == rhs.size &&
            path == rhs.path &&
            name == rhs.name &&
            mode == rhs.mode;
}
