
#include <QDebug>

#include "argcontrol_dbdelete.h"
#include "argcontrol_dbquery.h"
#include "qoutstream.h"

#include "qoptargparse.h"
#include "qoptsqlarg.h"
#include "database/db_controller.h"
#include "database/query_columns.h"
#include "cpp_exit.h"
#include "app.h"

using argcontol_dbquery::addVariantSqlArgToQueryIfParsed;
using db_controller::QueryColumns;


void argcontrol_dbdelete::parse(int argc, char *argv[])
{
    QOptArgParse parser;

    parser.setHelpIntroduction(qtr(
        "Delete commands (and all corresponding file events) from the database by id or date."));

    QOptSqlArg argCmdId("cmdid", "command-id", qtr("Deletes command with given id."),
                          {E_CompareOperator::EQ} );
    parser.addArg(&argCmdId);

    QOptSqlArg argCmdDate("cmded", "command-end-date", qtr("Deletes commands given by end-date. Example:\n"
                                                  "%1 --delete --command-end-date -between "
                                                  "2019-04-01 2019-04-02\n"
                                                  "deletes all commands which finished between "
                                                  "the first and second of April 2019.").arg(app::SHOURNAL),
                          QOptSqlArg::cmpOpsAllButLike() );
    parser.addArg(&argCmdDate);

    QOptArg argCmdOlderThan("", "older-than", qtr("Delete commands older than the given number of "
                                                  "years, months, days, etc.. Example:\n"
                                                  "%1 --delete --older-than 3y\n"
                                                  "deletes all commands which were executed more than "
                                                  "three years ago.").arg(app::SHOURNAL));
    argCmdOlderThan.setIsRelativeDateTime(true, true);
    parser.addArg(&argCmdOlderThan);

    QOptArg argCmdYoungerThan("", "younger-than", qtr("Delete commands younger than the given number of "
                                                  "years, months, days, etc.. Example:\n"
                                                  "%1 --delete --younger-than 1h\n"
                                                  "deletes all commands which were executed within "
                                                      "the last hour.").arg(app::SHOURNAL));
    argCmdYoungerThan.setIsRelativeDateTime(true, true);
    parser.addArg(&argCmdYoungerThan);


    parser.parse(argc, argv);
    SqlQuery query;

    auto & cols = QueryColumns::instance();

    addVariantSqlArgToQueryIfParsed<qint64>(query, argCmdId, cols.cmd_id);
    addVariantSqlArgToQueryIfParsed<QDateTime>(query, argCmdDate, cols.cmd_endtime);

    if(argCmdOlderThan.wasParsed()){
        auto olderThanDates = argCmdOlderThan.getVariantRelativeDateTimes();
        query.addWithAnd(cols.cmd_starttime, olderThanDates, E_CompareOperator::LT );
    }
    if(argCmdYoungerThan.wasParsed()){
        auto youngerThanDates = argCmdYoungerThan.getVariantRelativeDateTimes();
        query.addWithAnd(cols.cmd_starttime, youngerThanDates, E_CompareOperator::GT );
    }

    if( parser.rest().len != 0){
        QIErr() << qtr("Invalid parameters passed: %1.\n"
                       "Show help with --delete --help").
                   arg(argvToQStr(parser.rest().len, parser.rest().argv));
        cpp_exit(1);
    }

    if(query.isEmpty()){
        QIErr() << qtr("No target fields given (empty query).");
        cpp_exit(1);
    }

    QOut() << qtr("%1 command(s) deleted.").arg( db_controller::deleteCommand(query)) << "\n";


    cpp_exit(0);
}
