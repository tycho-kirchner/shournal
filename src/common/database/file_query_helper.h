#pragma once

#include <QFile>

#include "sqlquery.h"
#include "nullable_value.h"
#include "fileinfos.h"

namespace file_query_helper {
    SqlQuery buildFileQuerySmart(const QString& filename, bool readFile);
    SqlQuery buildFileQuery(const QString& filename, bool readFile,
                              bool use_mtime, bool use_hash, bool use_size);
}


