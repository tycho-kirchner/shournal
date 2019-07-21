#pragma once

#include <QFile>

#include "sqlquery.h"
#include "nullable_value.h"
#include "fileinfos.h"

namespace file_query_helper {
    void addWrittenFileSmart(SqlQuery& query, const QString& filename);
    void addWrittenFile(SqlQuery& query, const QString& filename,
                               bool mtime, bool hash_, bool size);
}


