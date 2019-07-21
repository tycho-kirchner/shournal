#include <QSqlDatabase>
#include <QSqlRecord>
#include <QSql>
#include <QSqlDriver>
#include <QDateTime>
#include <cassert>

#include "db_controller.h"
#include "db_connection.h"
#include "db_conversions.h"
#include "db_globals.h"
#include "qexcdatabase.h"
#include "qsqlquerythrow.h"
#include "query_columns.h"
#include "logger.h"
#include "util.h"
#include "cleanupresource.h"
#include "storedfiles.h"
#include "interrupt_handler.h"

using namespace db_conversions;

namespace  {

void
insertFileWriteEvents(const QueryPtr& query, qint64 cmdId,
                const FileWriteEventHash &writeEvents )
{
    query->prepare("insert into writtenFile (cmdId,path,name,mtime,size,hash) "
                  "values (?,?,?,?,?,?)");
    for(const auto& fileEvent : writeEvents) {
        query->addBindValue(cmdId);

        auto pathFnamePair =  splitAbsPath(QString::fromStdString(fileEvent.fullPath));
        query->addBindValue(pathFnamePair.first);
        query->addBindValue(pathFnamePair.second);
        query->addBindValue(fromMtime(fileEvent.mtime));

        query->addBindValue(static_cast<qint64>(fileEvent.size));
        query->addBindValue(fromHashValue(fileEvent.hash));
        query->exec();
    }
}


void
insertFileReadEvents(const QueryPtr& query, qint64 cmdId,
                     qint64 envId, const FileReadEventHash &readEvents )
{
    StoredFiles storedFiles;

    for(const auto& event : readEvents) {
        const auto pathFnamePair =  splitAbsPath(QString::fromStdString(event.fullPath));
        bool existed;
        const auto readFileId = query->insertIfNotExist("readFile", {
                                    {"envId", envId },
                                    {"name", pathFnamePair.second},
                                    {"path", pathFnamePair.first},
                                    {"mtime",fromMtime(event.mtime)},
                                    {"size", qint64(event.size)},
                                    {"mode", event.mode},
                                }, &existed);
        if(! existed){
            storedFiles.addReadFile(readFileId.toString(), event.bytes);
        }

        query->prepare("insert into readFileCmd (cmdId, readFileId) values (?,?)");
        query->addBindValue(cmdId);
        query->addBindValue(readFileId);
        query->exec();
    }

}





/// sql allows for cascade deleting orphans (children), here we kill
/// parents, where all children died
void
deleteChildlessParents(const QueryPtr& query){
    query->exec("delete from hashmeta where not exists "
               "(select 1 from cmd where cmd.hashmetaId=hashmeta.id)");
    query->exec("delete from session where not exists "
               "(select 1 from cmd where cmd.sessionId=session.id)");

    // delete stored read files (script files) in filesystem AND database
    query->setForwardOnly(true);
    query->exec("select readFile.id from readFile where not exists "
               "(select 1 from readFileCmd where readFileCmd.readFileId=readFile.id)");
    StoredFiles storedFiles;

    while(query->next()){
        const QString fname = query->value(0).toString();
        if(! storedFiles.deleteReadFile(fname) ){
            logWarning << qtr("failed to remove the file with name %1 "
                              "from the read files dir.").arg(fname);
        }
    }
    query->exec("delete from readFile where not exists "
               "(select 1 from readFileCmd where readFileCmd.readFileId=readFile.id)");

    // Do it last -> foreign key in readFile
    query->exec("delete from env where not exists (select 1 from cmd where "
               "cmd.envId=env.id)");

}



} // namespace




/////////////////////// public ////////////////////////////////


/// @return the new command id in database
/// @throws QExcDatabase
qint64 db_controller::addCommand(const CommandInfo &cmd)
{
    auto query = db_connection::mkQuery();
    query->transaction();


    query->prepare(query->insertIgnorePreamble() + " into env (hostname, username) values (?,?)");
    query->addBindValue(cmd.hostname);
    query->addBindValue(cmd.username);
    query->exec();

    query->prepare("select id from env where hostname=? and username=?");
    query->addBindValue(cmd.hostname);
    query->addBindValue(cmd.username);
    query->exec();
    query->next(true);
    const auto envId = qVariantTo_throw<qint64>(query->value(0));

    if(! cmd.hashMeta.isNull()) {
        query->prepare(query->insertIgnorePreamble() +
                      " into hashmeta (chunkSize, maxCountOfReads) values (?,?)");
        query->addBindValue(cmd.hashMeta.chunkSize);
        query->addBindValue(cmd.hashMeta.maxCountOfReads);
        query->exec();
    }

    if(! cmd.sessionInfo.uuid.isNull()) {
        query->prepare(query->insertIgnorePreamble() +
                      " into session (id) values (?)");
        query->addBindValue(cmd.sessionInfo.uuid);
        query->exec();
    }

    query->prepare("insert into cmd (txt,envId,hashmetaId,returnVal,"
                  "startTime,endTime,workingDirectory,sessionId) "
                  "values (?,?,"
                  "(select id from hashmeta where chunkSize=? and maxCountOfReads=?),"
                  "?,?,?,?,?)"
                  );
    query->addBindValue(cmd.text);
    query->addBindValue(envId);
    query->addBindValue(cmd.hashMeta.chunkSize);
    query->addBindValue(cmd.hashMeta.maxCountOfReads);
    query->addBindValue(cmd.returnVal);
    query->addBindValue(cmd.startTime);
    query->addBindValue(cmd.endTime);
    query->addBindValue(cmd.workingDirectory);
    query->addBindValue(cmd.sessionInfo.uuid);
    query->exec();

    return qVariantTo_throw<qint64>(query->lastInsertId());
}



/// update only relevant command fields, which are those that are not
/// known from the beginning. The startTime changes in case of an empty entered command
/// within the shell integration.
void db_controller::updateCommand(const CommandInfo &cmd)
{
    assert(cmd.idInDb != db::INVALID_INT_ID);
    auto query = db_connection::mkQuery();

    query->prepare("update cmd set txt=?,returnVal=?,startTime=?,endTime=? "
                   "where `id`=?");
    query->addBindValue(cmd.text);
    query->addBindValue(cmd.returnVal);
    query->addBindValue(cmd.startTime);
    query->addBindValue(cmd.endTime);
    query->addBindValue(cmd.idInDb);

    query->exec();

}




void db_controller::addFileEvents(qint64 cmdId, const FileWriteEventHash &writeEvents,
                                  const FileReadEventHash &readEvents)
{
    auto query = db_connection::mkQuery();
    query->transaction();

    query->prepare("select envId from cmd where `id`=?");
    query->addBindValue(cmdId);
    query->exec();
    query->next(true);
    const auto envId = qVariantTo_throw<qint64>(query->value(0));

    insertFileWriteEvents(query, cmdId, writeEvents);
    insertFileReadEvents(query, cmdId, envId, readEvents);
}



/// Deletes the command and corresponding file events (read and write).
/// @param sqlQuery: may only refer to columns of the 'cmd'-table.
/// @returns numRowsAffected
int db_controller::deleteCommand(const SqlQuery &sqlQuery)
{
    auto query = db_connection::mkQuery();
    query->transaction();

    query->prepare("delete from cmd where " + sqlQuery.query());
    query->addBindValues(sqlQuery.values());

    InterruptProtect ip;

    query->exec();
    int numRowsAffected = query->numRowsAffected();
    // the respective triggers have also caused the deletion of orphans in
    // writtenFile, readFileCmd, etc., however, we still need to handle childless parents:
    deleteChildlessParents(query);
    return numRowsAffected;
}


/// @param reverseResultIter: if true, the returned Iterator will traverse the resultset in
/// reverse order on continous 'next'-calls. Reversing the iterator might cause performance-issues,
/// since the whole resultset will need to be stored in memory
std::unique_ptr<CommandQueryIterator>
db_controller::queryForCmd(const SqlQuery &sqlQ, bool reverseResultIter){
    auto pQuery = db_connection::mkQuery();
    std::unique_ptr<CommandQueryIterator> cmdIter(
                new CommandQueryIterator(pQuery, reverseResultIter));

    const QString queryStr =
            "select cmd.id, cmd.txt, "
            "cmd.returnVal, cmd.startTime, cmd.endTime, cmd.workingDirectory,"
            "session.id, session.comment,"
            "hashmeta.chunkSize, hashmeta.maxCountOfReads,"
            "env.username, env.hostname "
            "from cmd "             +
            QString((sqlQ.containsTablename("writtenFile")) ?
                        "join writtenFile on cmd.id=writtenFile.cmdId " : "") +
            QString((sqlQ.containsTablename("readFile")) ?
                        "join readFileCmd on cmd.id=readFileCmd.cmdId "
                        "join readFile on readFileCmd.readFileId=readFile.id ":
                        "") +
            "join env on cmd.envId=env.id "
            "left join hashmeta on hashmeta.id=cmd.hashmetaId " // left joins last, if possible!
            "left join `session` on cmd.sessionId=session.id "
            "where ";

    // endTime is currently more accurate than startTime (at least
    // when collected within the shell integration
    const QString orderBy = "order by cmd.endTime " +
                        QString((sqlQ.ascending()) ? "asc " : "desc ") +
                        ((sqlQ.limit() == -1) ? "" : "limit " + QString::number(sqlQ.limit()));
    if( ! reverseResultIter){
        pQuery->setForwardOnly(true);
    }
    pQuery->prepare(queryStr + sqlQ.query() + " group by cmd.id " + orderBy);
    pQuery->addBindValues(sqlQ.values());

    pQuery->exec();
    if(reverseResultIter){
        // place cursor right after the last record, so a call to "previous" points to last.
        pQuery->last();
        pQuery->next();
    }
    return cmdIter;
}

/// if no entry can be found, the id of the returned file info is invalid.
FileReadInfo db_controller::queryReadInfo(const qint64 id)
{
    auto query = db_connection::mkQuery();
    query->prepare("select readFile.id,path,name,mtime,size,mode from readFile "
                  "where readFile.id=?");
    query->addBindValue(id);
    query->exec();
    FileReadInfo info;
    if(! query->next()){
        return info;
    }
    int i=0;
    info.idInDb = qVariantTo_throw<qint64>(query->value(i++));
    info.path = query->value(i++).toString();
    info.name = query->value(i++).toString();
    info.mtime = query->value(i++).toDateTime();
    info.size =  qVariantTo_throw<qint64>(query->value(i++));
    info.mode =  qVariantTo_throw<mode_t>(query->value(i++));

    return info;
}

/// @param restrictingFilesize: only return hashmeta-entries for which at least one file
/// exists which was recorded using a given hashmeta and whose size is exactly that.
db_controller::HashMetas
db_controller::queryHashmetas(qint64 restrictingFilesize){

    auto query = db_connection::mkQuery();
    query->prepare("select chunkSize, maxCountOfReads from cmd "
                  "join `writtenFile` on cmd.id=writtenFile.cmdId "
                  "left join hashmeta on cmd.hashmetaId=hashmeta.id "
                  "where writtenFile.size=? "
                  "group by chunkSize,maxCountOfReads ");
    query->addBindValue(restrictingFilesize);
    query->exec();
    db_controller::HashMetas hashMetas;
    bool noHashAdded = false;
    while(query->next()){
        if( query->value(0).isNull()){
            if( ! noHashAdded){
                hashMetas.push_back(HashMeta());
                noHashAdded = true;
            }
        } else {
            HashMeta h;
            qVariantTo_throw(query->value(0), &h.chunkSize);
            qVariantTo_throw(query->value(1), &h.maxCountOfReads);
            hashMetas.push_back(h);
        }
    }
    return hashMetas;
}

