#pragma once

#include "sqlquery.h"
#include "qoptsqlarg.h"

namespace argcontol_dbquery {
    [[noreturn]]
    void parse(int argc, char *argv[]);

    template <class T>
    void addSimpleSqlArgToQueryIfParsed(SqlQuery& query, QOptSqlArg& arg, const QString& tableCol);

    template <class T>
    void addVariantSqlArgToQueryIfParsed(SqlQuery& query,
                                         QOptSqlArg& arg, const QString& tableCol);

    void addBytesizeSqlArgToQueryIfParsed(SqlQuery& query,
                                         QOptSqlArg& arg, const QString& tableCol);
}



template <class T>
void argcontol_dbquery::addSimpleSqlArgToQueryIfParsed(SqlQuery& query,
                                                       QOptSqlArg& arg, const QString& tableCol){
    if(! arg.wasParsed()){
        return;
    }

    query.addWithAnd(tableCol,
                     arg.getValue<T>(),
                     arg.parsedOperator() );
}

template <class T>
void argcontol_dbquery::addVariantSqlArgToQueryIfParsed(SqlQuery& query,
                                                       QOptSqlArg& arg, const QString& tableCol){
    if(! arg.wasParsed()){
        return;
    }

    query.addWithAnd(tableCol,
                     arg.getVariantValues<T>(),
                     arg.parsedOperator() );
}
