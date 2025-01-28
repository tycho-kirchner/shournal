#include "fileinfos.h"

#include "db_conversions.h"
#include "commandinfo.h"
#include "hashcontrol.h"
#include "logger.h"
#include "qfilethrow.h"
#include "util.h"

FileInfo::~FileInfo() {};

QString FileInfo::currentStatus(const CommandInfo &cmd) const
{
    auto filename = pathJoinFilename(this->path, this->name);
    try{
        QFileThrow f(filename);
        if(!f.exists()){
            return "N";
        }
        HashControl hashCtrl;
        f.open(QFile::OpenModeFlag::ReadOnly);
        const auto st_ = os::fstat(f.handle());
        if(size != st_.st_size ||
           QDateTime::fromTime_t(static_cast<uint>(st_.st_mtime))!= mtime ||
           hash!= hashCtrl.genPartlyHash(f.handle(), st_.st_size, cmd.hashMeta, false)){
            return "M";
        }
        return "U";
    } catch (const std::exception& ex) {
        logWarning << qtr("Failed to determine status of file %1 - %2").arg(filename)
                      .arg(QString(ex.what()));
        return "ERROR";
    }
}


void FileWriteInfo::write(QJsonObject &json) const
{
    json["id"] = idInDb;
    json["path"] = pathJoinFilename(path, name);
    json["size"] = size;
    json["mtime"] = QJsonValue::fromVariant(mtime);
    json["hash"] = QJsonValue::fromVariant(QVariant::fromValue(hash));
}

bool
FileWriteInfo::operator==(const FileInfo &rhs) const
{
    if(idInDb != db::INVALID_INT_ID && rhs.idInDb != db::INVALID_INT_ID){
        return idInDb == rhs.idInDb;
    }
    return mtime == rhs.mtime &&
            size == rhs.size &&
            path == rhs.path &&
            name == rhs.name &&
            hash == rhs.hash;
}

////////////////////////////////////////////////////////////

void FileReadInfo::write(QJsonObject &json) const
{
    json["id"] = idInDb;
    json["path"] = pathJoinFilename(path, name);
    json["size"] = size;
    json["mtime"] = QJsonValue::fromVariant(mtime);
    // Note: in case of a non-null hash, this results in a quoted string.
    // While useful in the html-export (javascript INT-limit..), technically this is not totally
    // correct. However, it has always been so, so do not change.
    json["hash"] = QJsonValue::fromVariant(QVariant::fromValue(hash));
    json["isStoredToDisk"] = isStoredToDisk;
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

bool FileReadInfo::operator==(const FileInfo&) const
{
    throw QExcProgramming("Unimplemented FileReadInfo::operator==(const FileInfo &rhs)");
}
