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

void addFileEvents(qint64 cmdId, const FileWriteEventHash &writeEvents,
                   const FileReadEventHash &readEvents);

int deleteCommand(const SqlQuery &query);

std::unique_ptr<CommandQueryIterator> queryForCmd(const SqlQuery& query, bool reverseResultIter=false);

FileReadInfo queryReadInfo(const qint64 id);

HashMetas queryHashmetas(qint64 restrictingFilesize);


}







