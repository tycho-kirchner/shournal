
#include <QDebug>
#include "command_query_iterator.h"
#include "util.h"
#include "db_connection.h"
#include "db_conversions.h"
#include "db_controller.h"


///  @param reverseIter: if true, instead of calling next(), previous() will be called
/// on the passed query.
CommandQueryIterator::CommandQueryIterator(std::shared_ptr<QSqlQueryThrow>& query, bool reverseIter) :
    m_cmdQuery(query),
    m_tmpQuery(db_connection::mkQuery()),
    m_reverseIter(reverseIter)
{
}

// set cursor to next or previous, if reverseIter was set on constructor
bool CommandQueryIterator::next()
{
    m_cmd.clear();
    const bool nextRet = (m_reverseIter) ? m_cmdQuery->previous() : m_cmdQuery->next();
    if(nextRet){
        fillCommand();
    }
    return nextRet;
}

CommandInfo &CommandQueryIterator::value()
{
    return m_cmd;
}



void CommandQueryIterator::fillCommand()
{
    int i=0;
    m_cmd.idInDb = qVariantTo_throw<qint64>(m_cmdQuery->value(i++));
    m_cmd.text = m_cmdQuery->value(i++).toString();
    m_cmd.returnVal = m_cmdQuery->value(i++).toInt();
    m_cmd.startTime = m_cmdQuery->value(i++).toDateTime();
    m_cmd.endTime = m_cmdQuery->value(i++).toDateTime();
    m_cmd.workingDirectory = m_cmdQuery->value(i++).toString();

    m_cmd.sessionInfo.uuid = m_cmdQuery->value(i++).toByteArray();
    m_cmd.sessionInfo.comment = m_cmdQuery->value(i++).toString();

    QVariant hashChunksize = m_cmdQuery->value(i++);
    if(! hashChunksize.isNull()){
        qVariantTo_throw(hashChunksize, &m_cmd.hashMeta.chunkSize) ;
        qVariantTo_throw(m_cmdQuery->value(i++), &m_cmd.hashMeta.maxCountOfReads);
    } else {
        i++;
    }
    m_cmd.username = m_cmdQuery->value(i++).toString();
    m_cmd.hostname = m_cmdQuery->value(i++).toString();

    fillWrittenFiles();
    m_cmd.fileReadInfos = db_controller::queryReadInfos_byCmdId(m_cmd.idInDb);
}

void CommandQueryIterator::fillWrittenFiles()
{
    m_tmpQuery->prepare("select path,name,mtime,size,hash from writtenFile where cmdId=?");
    m_tmpQuery->addBindValue(m_cmd.idInDb);
    m_tmpQuery->exec();
    while(m_tmpQuery->next()){
        int i=0;
        FileWriteInfo fInfo;
        fInfo.path = m_tmpQuery->value(i++).toString();
        fInfo.name = m_tmpQuery->value(i++).toString();
        fInfo.mtime = m_tmpQuery->value(i++).toDateTime();
        fInfo.size =  qVariantTo_throw<qint64>(m_tmpQuery->value(i++));
        fInfo.hash = db_conversions::toHashValue(m_tmpQuery->value(i++));
        m_cmd.fileWriteInfos.push_back(fInfo);
    }
}



