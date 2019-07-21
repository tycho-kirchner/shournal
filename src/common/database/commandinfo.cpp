
#include <QHostInfo>
#include <QString>

#include "commandinfo.h"

#include "os.h"
#include "settings.h"
#include "db_globals.h"

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
    auto & wsets = Settings::instance().writeFileSettings();
    if(wsets.hashEnable){
        cmd.hashMeta = wsets.hashMeta;
    }
    return cmd;
}

CommandInfo::CommandInfo()
    : idInDb(db::INVALID_INT_ID),
      text(""), // empty string, so QString.isNull() returns false -> no null-inserts into database
      returnVal(INVALID_RETURN_VAL)
{}

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



