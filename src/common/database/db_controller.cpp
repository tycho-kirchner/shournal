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
#include "os.h"

using namespace db_conversions;

namespace  {

void
insertFileWriteEvents(const QueryPtr& query, const CommandInfo &cmd,
                FileWriteEvents &writeEvents )
{
    query->prepare("insert into writtenFile (cmdId,path,name,mtime,size,hash) "
                  "values (?,?,?,?,?,?)");

    FileWriteEvent* e;
    while ((e = writeEvents.read()) != nullptr) {
        query->addBindValue(cmd.idInDb);

        auto pathFnamePair =  splitAbsPath(QString(e->fullPath));
        query->addBindValue(pathFnamePair.first);
        query->addBindValue(pathFnamePair.second);
        query->addBindValue(fromMtime(e->mtime));

        query->addBindValue(static_cast<qint64>(e->size));
        HashValue hash;
        if(! e->hashIsNull){
            hash = e->hash;
        }
        query->addBindValue(fromHashValue(hash));
        query->exec();
    }
}

/// Move or copy the file captured along the read event e
/// to the read files directory in shournal's database dir.
void moveOrCopyToStoredFiles(const FileReadEvent& e, FileReadEvents& readEvents,
                             const QByteArray& storedFilesDir,
                             const QByteArray& idInDatabase, bool onSameDev){
    const auto fullDestPath = storedFilesDir + '/' + idInDatabase;
    const auto fullSrcPath = readEvents.makeStoredFilepath(e);
    try {
        if(onSameDev){
            os::rename(fullSrcPath, fullDestPath);
        } else {
            os::sendfile(fullDestPath, fullSrcPath, e.size);
        }
    } catch (const os::ExcOs& ex) {
        logCritical << QString("Failed to move or copy %1 to %2: %3")
                       .arg(fullSrcPath.constData())
                       .arg(fullDestPath.constData())
                       .arg(ex.what());
        throw;
    }


}

void
insertFileReadEvents(const QueryPtr& query, const CommandInfo &cmd,
                     const QVariant& envId, const QVariant& hashMetaId,
                     FileReadEvents &readEvents )
{
    StoredFiles storedFiles;
    const QByteArray storedFilesDir = storedFiles.getReadFilesDir().toUtf8();
    const auto devTmpFiles = os::stat(strDataAccess(readEvents.getTmpDirPath())).st_dev;
    const auto devStoredFiles = os::stat(strDataAccess(storedFiles.getReadFilesDir().toUtf8())).st_dev;
    const bool tmpAndStoredFilesOnSameDev = devTmpFiles == devStoredFiles;

    FileReadEvent* e;
     while ((e = readEvents.read()) != nullptr) {
        const auto pathFnamePair =  splitAbsPath(QString(e->fullPath));
        bool existed;
        HashValue hash;
        if(! e->hashIsNull){
            hash = e->hash;
        }
        const auto readFileId = query->insertIfNotExist("readFile", {
                                    {"envId", envId },
                                    {"name", pathFnamePair.second},
                                    {"path", pathFnamePair.first},
                                    {"mtime",fromMtime(e->mtime)},
                                    {"size", qint64(e->size)},
                                    {"mode", e->mode},
                                    {"hash", fromHashValue(hash)},
                                    {"hashmetaId", hashMetaId},
                                    {"isStoredToDisk", e->file_content_id != -1}
                                }, &existed);
        if(! existed && e->file_content_id != -1){
            moveOrCopyToStoredFiles(*e, readEvents, storedFilesDir,
                                    readFileId.toByteArray(), tmpAndStoredFilesOnSameDev);
        }
        query->prepare("insert into readFileCmd (cmdId, readFileId) values (?,?)");
        query->addBindValue(cmd.idInDb);
        query->addBindValue(readFileId);
        query->exec();
    }
}





/// sql allows for cascade deleting orphans (children), here we kill
/// parents, where all children died
void
deleteChildlessParents(const QueryPtr& query){
    logDebug << "delete from hashmeta...";
    query->exec("delete from hashmeta where not exists "
               "(select 1 from cmd where cmd.hashmetaId=hashmeta.id)");
    logDebug << "delete from session...";
    query->exec("delete from session where not exists "
               "(select 1 from cmd where cmd.sessionId=session.id)");

    // delete stored read files (script files) in filesystem AND database
    query->setForwardOnly(true);
    query->prepare("select readFile.id from readFile where "
        "readFile.isStoredToDisk=? and "
        "not exists (select 1 from readFileCmd where readFileCmd.readFileId=readFile.id) ");
    query->bindValue(0, true);
    query->exec();
    StoredFiles storedFiles;
    logDebug << "looping though read 'script' files to evtl. delete from filesystem...";
    while(query->next()){
        const QString fname = query->value(0).toString();
        if(! storedFiles.deleteReadFile(fname) ){
            logWarning << qtr("failed to remove the file with name %1 "
                              "from the read files dir.").arg(fname);
        }
    }
    logDebug << "delete from readFile...";
    query->exec("delete from readFile where not exists "
               "(select 1 from readFileCmd where readFileCmd.readFileId=readFile.id)");

    // Do it last -> foreign key in readFile
    logDebug << "delete from env...";
    query->exec("delete from env where not exists (select 1 from cmd where "
               "cmd.envId=env.id)");
}


FileReadInfos
queryFileReadInfos(const SqlQuery& sqlQ, const QueryPtr& query_=nullptr, const QString& optionalJoins={}){
    const QueryPtr query = (query_ != nullptr) ? query_ : db_connection::mkQuery();
    FileReadInfos readInfos;
    query->prepare("select readFile.id,path,name,mtime,size,mode,hash,isStoredToDisk from readFile "
                   + optionalJoins + " where " + sqlQ.query());
    query->addBindValues(sqlQ.values());
    query->exec();
    while(query->next()){
        int i=0;
        FileReadInfo fInfo;
        fInfo.idInDb = qVariantTo_throw<qint64>(query->value(i++));
        fInfo.path = query->value(i++).toString();
        fInfo.name = query->value(i++).toString();
        fInfo.mtime = query->value(i++).toDateTime();
        fInfo.size =  qVariantTo_throw<qint64>(query->value(i++));
        fInfo.mode =  qVariantTo_throw<mode_t>(query->value(i++));
        fInfo.hash = db_conversions::toHashValue(query->value(i++));
        fInfo.isStoredToDisk = query->value(i++).toBool();

        readInfos.push_back(fInfo);
    }
    return readInfos;
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
/// known from the beginning.
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


/// Add file events belonging to param cmd which must belong to a valid
/// database entry (idInDb must valid)
void db_controller::addFileEvents(const CommandInfo &cmd, FileWriteEvents &writeEvents,
                                  FileReadEvents &readEvents)
{
    assert(cmd.idInDb != db::INVALID_INT_ID);

    auto query = db_connection::mkQuery();
    query->transaction();

    query->prepare("select envId,hashmetaId from cmd where `id`=?");
    query->addBindValue(cmd.idInDb);
    query->exec();
    query->next(true);
    const QVariant envId = query->value(0);
    const QVariant hashMetaId = query->value(1);

    insertFileWriteEvents(query, cmd, writeEvents);
    insertFileReadEvents(query, cmd, envId, hashMetaId, readEvents);
}



/// Deletes the command and corresponding file events (read and write).
/// @param sqlQuery: may only refer to columns of the 'cmd'-table.
/// @returns numRowsAffected
int db_controller::deleteCommand(const SqlQuery &sqlQuery)
{
    auto query = db_connection::mkQuery();
    query->transaction();

    logDebug << "deleting cmd" << sqlQuery.query();
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
/// reverse order on continous 'next'-calls.
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
                        "join readFile on readFileCmd.readFileId=readFile.id " :
                        "") +
            "join env on cmd.envId=env.id "
            "left join hashmeta on hashmeta.id=cmd.hashmetaId " // left joins last, if possible!
            "left join `session` on cmd.sessionId=session.id "
            "where ";

    // do not change this -> order matters in html-plot...
    const QString orderBy = "order by cmd.startTime " + sqlQ.ascendingStr() +
                            sqlQ.mkLimitString();

    // we need the size (at other places) but QSQLITE does not support QSqlQuery::size.
    // To use a workaround, forward mode must not be enabled.
    // See also https://stackoverflow.com/a/26500811/7015849
    // if( ! reverseResultIter){
    //     pQuery->setForwardOnly(true);
    // }
    const QString fullQuery = queryStr + sqlQ.query() + " group by cmd.id " + orderBy;
    pQuery->prepare(fullQuery);
    pQuery->addBindValues(sqlQ.values());
    logDebug << "executing" << fullQuery;
    pQuery->exec();

    if(reverseResultIter){
        // place cursor right after the last record, so a call to "previous" points to last.
        pQuery->last();
        pQuery->next();
    }
    return cmdIter;
}

/// if no entry can be found, the id of the returned file info is invalid.
FileReadInfo db_controller::queryReadInfo_byId(const qint64 id, const QueryPtr& query_)
{
    SqlQuery sqlQ;
    sqlQ.addWithAnd("readFile.id", id);
    auto fileReadInfos = queryFileReadInfos(sqlQ, query_);
    if(fileReadInfos.isEmpty()){
        return FileReadInfo();
    }
    assert(fileReadInfos.size() == 1);
    return fileReadInfos.first();
}

FileReadInfos db_controller::queryReadInfos_byCmdId(qint64 cmdId, const QueryPtr &query_)
{
    SqlQuery sqlQ;
    sqlQ.addWithAnd("cmdId", cmdId);
    return queryFileReadInfos(sqlQ, query_,
                              " join readFileCmd on "
                              "readFile.id=readFileCmd.readFileId ");
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



