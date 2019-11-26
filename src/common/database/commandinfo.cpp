
#include <QHostInfo>
#include <QString>
#include <QJsonArray>

#include "commandinfo.h"

#include "os.h"
#include "settings.h"
#include "db_globals.h"
#include "conversions.h"

/// Settings must be loaded beforehand!
/// Fill commandInfo with those information independent from the current
/// command. The following properties yet *have* to be set:
/// startTime, endTime, text
/// The following *may* be set:
/// returnVal
CommandInfo CommandInfo::fromLocalEnv()
{
    CommandInfo cmd;

    cmd.hostname = QHostInfo::localHostName();
    cmd.username = os::getUserName<QString>();
    // Do not:
    // cmd.workingDirectory = QDir::currentPath();
    // If the working directory is deleted, this returns a null string, which is not very
    // informative (and also not allowed in the database scheme). Using below approach returns
    // a valid string with a trailing ' (deleted)', if appropriate.
    cmd.workingDirectory = QString::fromLocal8Bit(os::readlink<QByteArray>("/proc/self/cwd"));
    auto & sets = Settings::instance();
    if(sets.hashSettings().hashEnable){
        cmd.hashMeta = sets.hashSettings().hashMeta;
    }
    return cmd;
}

CommandInfo::CommandInfo()
    : idInDb(db::INVALID_INT_ID),
      text(""), // empty string, so QString.isNull() returns false -> no null-inserts into database
      returnVal(INVALID_RETURN_VAL)
{}

void CommandInfo::write(QJsonObject &json, bool withMilliseconds,
                        const CmdJsonWriteCfg &writeCfg) const
{
    if(writeCfg.idInDb) json["id"] = idInDb;
    if(writeCfg.text) json["command"] = text;
    if(writeCfg.returnVal) json["returnValue"] = returnVal;
    if(writeCfg.username) json["username"] = username;
    if(writeCfg.hostname) json["hostname"] = hostname;

    if(writeCfg.hashMeta) {
        QJsonValue hashChunkSize;
        QJsonValue hashMaxCountOfReads;
        if(! hashMeta.isNull()){
            hashChunkSize = hashMeta.chunkSize;
            hashMaxCountOfReads = hashMeta.maxCountOfReads;
        }
        json["hashChunkSize"] = hashChunkSize;
        json["hashMaxCountOfReads"] = hashMaxCountOfReads;
    }

    // A null-session-QString becomes a quoted string in json, instead of null, so below
    // effort is necessary (invalid session should always be null: in database, shournal and js-plot...).
    if(writeCfg.sessionInfo){
        json["sessionUuid"] = (sessionInfo.uuid.isNull()) ? QJsonValue() :
                                                        QString::fromLatin1(sessionInfo.uuid.toBase64());
    }

    if(withMilliseconds){
        if(writeCfg.startEndTime){
            json["startTime"] = startTime.toString(Conversions::dateIsoFormatWithMilliseconds());
            json["endTime"] = endTime.toString(Conversions::dateIsoFormatWithMilliseconds());
        }
    } else {
        if(writeCfg.startEndTime){
            json["startTime"] = QJsonValue::fromVariant(startTime);
            json["endTime"] = QJsonValue::fromVariant(endTime);
        }
    }
    if(writeCfg.workingDirectory) json["workingDir"] = workingDirectory;

    if(writeCfg.fileReadInfos){
        QJsonArray fReadArr;
        int idx = 0;
        for(const auto& info : fileReadInfos){

            QJsonObject fReadObj;
            info.write(fReadObj);
            fReadArr.append(fReadObj);
            ++idx;
            if(idx >= writeCfg.maxCountRFiles){
                break;
            }
        }
        json["fileReadEvents"] = fReadArr;
    }

    if(writeCfg.fileWriteInfos){
        QJsonArray fWriteArr;
        int idx = 0;
        for(const auto& info : fileWriteInfos){
            QJsonObject fWriteObject;
            info.write(fWriteObject);
            fWriteArr.append(fWriteObject);
            ++idx;
            if(idx >= writeCfg.maxCountWFiles){
                break;
            }
        }
        json["fileWriteEvents"] = fWriteArr;
    }

}

bool CommandInfo::operator==(const CommandInfo &rhs) const
{
    if(idInDb != db::INVALID_INT_ID && rhs.idInDb != db::INVALID_INT_ID){
        return idInDb == rhs.idInDb;
    }

    return text == rhs.text &&
           returnVal == rhs.returnVal &&
           username == rhs.username &&
           hostname == rhs.hostname &&
           hashMeta == rhs.hashMeta &&
           sessionInfo == rhs.sessionInfo &&
           fileWriteInfos == rhs.fileWriteInfos &&
           fileReadInfos == rhs.fileReadInfos &&
           startTime == rhs.startTime &&
           endTime == rhs.endTime &&
           workingDirectory == rhs.workingDirectory;
}

void CommandInfo::clear()
{
    fileWriteInfos.clear();
    fileReadInfos.clear();
    idInDb = db::INVALID_INT_ID;
}



