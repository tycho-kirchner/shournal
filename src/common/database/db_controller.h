#pragma once

#include <QByteArray>
#include <QVector>
#include <memory>

#include "fileeventtypes.h"
#include "commandinfo.h"
#include "sqlquery.h"
#include "db_connection.h"
#include "qsqlquerythrow.h"
#include "command_query_iterator.h"


namespace db_controller {

typedef QVector<HashMeta> HashMetas;

qint64 addCommand(const CommandInfo &cmd);
void updateCommand(const CommandInfo &cmd);

void addFileEvents(const CommandInfo &cmd, FileWriteEvents &writeEvents,
                   FileReadEvents &readEvents);

int deleteCommand(const SqlQuery &query);

std::unique_ptr<CommandQueryIterator> queryForCmd(const SqlQuery& sqlQ, bool reverseResultIter=false);

FileReadInfo queryReadInfo_byId(qint64 id, const QueryPtr& query_=nullptr);
FileReadInfos queryReadInfos_byCmdId(qint64 cmdId, const QueryPtr& query_=nullptr);

HashMetas queryHashmetas(qint64 restrictingFilesize);


}







