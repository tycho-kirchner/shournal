#pragma once

#include "qsqlquerythrow.h"

namespace sqlite_database_scheme_updates {
    void v0_9(QSqlQueryThrow& query); // 0.8 -> 0.9
    void v2_1(QSqlQueryThrow& query); // 2.0 -> 2.1
    void v2_2(QSqlQueryThrow& query); // 2.1 -> 2.2

}

