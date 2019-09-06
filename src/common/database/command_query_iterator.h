#pragma once

#include <memory>

#include "qsqlquerythrow.h"
#include "commandinfo.h"
#include "db_connection.h"

class CommandQueryIterator
{
public:
    CommandQueryIterator(std::shared_ptr<QSqlQueryThrow> &query, bool reverseIter);

    bool next();

    CommandInfo& value();

public:
    CommandQueryIterator(const CommandQueryIterator &) = delete ;
    void operator=(const CommandQueryIterator &) = delete ;

private:

    void fillCommand();
    void fillWrittenFiles();

    std::shared_ptr<QSqlQueryThrow> m_cmdQuery;
    QueryPtr m_tmpQuery;
    CommandInfo m_cmd;
    bool m_reverseIter;
};

