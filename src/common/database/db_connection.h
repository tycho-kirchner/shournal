#pragma once

#include <memory>

#include "qsqlquerythrow.h"

typedef std::shared_ptr<QSqlQueryThrow> QueryPtr;

namespace db_connection {

const QString &getDatabaseDir();
QString mkDbPath();

void setupIfNeeded();
QueryPtr mkQuery();

void close();
}

