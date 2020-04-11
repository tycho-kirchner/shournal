#include <QDebug>
#include <cassert>
#include <unistd.h>

#include "argcontrol_dbquery.h"

#include "qoptargparse.h"
#include "qoptsqlarg.h"
#include "database/db_globals.h"
#include "database/db_controller.h"
#include "database/query_columns.h"
#include "database/file_query_helper.h"
#include "database/db_conversions.h"
#include "app.h"
#include "logger.h"
#include "qoutstream.h"
#include "cpp_exit.h"
#include "command_printer.h"
#include "command_printer_human.h"
#include "command_printer_json.h"
#include "command_printer_html.h"
#include "console_dialog.h"
#include "osutil.h"
#include "translation.h"
#include "conversions.h"

using translation::TrSnippets;

using db_controller::QueryColumns;

namespace  {
[[noreturn]]
void
queryCmdPrintAndExit(std::unique_ptr<CommandPrinter>& cmdPrinter,
                          SqlQuery& sqlQ,
                          bool reverseResultIter ){
    auto results = db_controller::queryForCmd(sqlQ, reverseResultIter);
    cmdPrinter->printCommandInfosEvtlRestore(results);
    cpp_exit(0);
}

[[noreturn]]
void
restoreSingleReadFile(QOptArg& argRestoreRfileId){
    auto fReadInfo = db_controller::queryReadInfo_byId(
                static_cast<qint64>(argRestoreRfileId.getValue<uint64_t>())
                );
    if(fReadInfo.idInDb == db::INVALID_INT_ID){
        QIErr() << qtr("cannot restore file - no database-entry exists");
        cpp_exit(1);
    }
    if(! fReadInfo.isStoredToDisk){
        QIErr() << qtr("cannot restore file %1 - only meta-information (path, name, etc.) "
                       "about the file is stored in the database but not the "
                       "file itself.").arg(fReadInfo.name);
        cpp_exit(1);
    }

    QDir currentDir = QDir::current();
    if(QFile::exists(currentDir.absoluteFilePath(fReadInfo.name)) &&
       osutil::isTTYForegoundProcess(STDIN_FILENO) &&
       ! console_dialog::yesNo(qtr("File %1 exists. Replace?").arg(fReadInfo.name)) ) {
        cpp_exit(0);
    }
    StoredFiles().restoreReadFileAtDIr(fReadInfo, currentDir);
    QOut() << qtr("File '%1' restored at current working directory.").arg(fReadInfo.name) << "\n";
    cpp_exit(0);
}

} // namespace


void argcontol_dbquery::addBytesizeSqlArgToQueryIfParsed(SqlQuery &query, QOptSqlArg &arg,
                                                         const QString &tableCol)
{
    if(! arg.wasParsed()) return;

    query.addWithAnd(tableCol, arg.getVariantByteSizes(), arg.parsedOperator() );
}

void argcontol_dbquery::parse(int argc, char *argv[])
{
    QOptArgParse parser;

    parser.setHelpIntroduction(qtr(
        "Query the command/file-database for several parameters which are\n"
        "AND-connected. For several fields optional comparison-operators are supported.\n"
        "The operators are passed in shell-friendly syntax so e.g. "
        "-gt stands for 'greater than'.\n"
        "-like will allow for using sql wildcards (e.g. '%').\n"
        "Examples:\n"
        "%1 --query --wfile /tmp/foo123 - use existing file to find out, how it was created.\n"
        "%1 --query --wsize -gt 10KiB - print all commands which have written to files whose "
                                    "size is greater than 10KiB.\n"
        "%1 --query --wpath -like /home/user% - print all commands, which have written to files "
                                     "below /home/user and all subdirectories.\n"
                                   ).arg(app::SHOURNAL) + "\n");

    QOptArg argHistory("", "history",
                            qtr("Only display the last N commands, you may optionally "
                                "filter by other parameters as well (like command-text)")
                            );
    parser.addArg(&argHistory);

    // ------------ wfile
    QOptArg argWFile("wf", "wfile",
                    qtr("Pass an existing file(-path) to find out the command, "
                        "which caused the creation/modification of a given file "
                        "(wfile stands for 'written file'). Per default the query is performed on "
                        "the basis of hash(es), mtime and size." ));
    parser.addArg(&argWFile);

    QOptArg argTakeFromWFile("", "take-from-wfile",
                            qtr("Specify explicitly which properties to collect "
                                "from the given file passed via %1. "
                                "Typically you do not need this.").arg(argWFile.name())
                            );
    argTakeFromWFile.addRequiredArg(&argWFile);
    argTakeFromWFile.setAllowedOptions({"mtime", "hash", "size"});
    parser.addArg(&argTakeFromWFile);

    const QString wFilePreamble = qtr("Query for files written to ");
    QOptSqlArg argWName("wn", "wname", wFilePreamble + qtr("by filename."),
                        QOptSqlArg::cmpOpsText());
    parser.addArg(&argWName);

    QOptSqlArg argWPath("wp", "wpath", wFilePreamble + qtr("by (full) directory-path."),
                        QOptSqlArg::cmpOpsText());
    parser.addArg(&argWPath);

    QOptSqlArg argWSize("ws", "wsize", wFilePreamble + qtr("by filesize."),
                       QOptSqlArg::cmpOpsAllButLike() );
    argWSize.setIsByteSizeArg(true);
    parser.addArg(&argWSize);

    QOptSqlArg argWHash("wh", "whash", wFilePreamble + qtr("by hash."),
                        QOptSqlArg::cmpOpsEqNe() );
    parser.addArg(&argWHash);

    QOptSqlArg argWMtime("wm", "wmtime", wFilePreamble + qtr("by mtime."),
                       QOptSqlArg::cmpOpsAllButLike() );
    parser.addArg(&argWMtime);

    // ------------ rfile

    const QString rFilePreamble = qtr("Query for read files ");
    QOptSqlArg argRName("rn", "rname", rFilePreamble + qtr("by filename."),
                        QOptSqlArg::cmpOpsText());
    parser.addArg(&argRName);

    QOptSqlArg argRPath("rp", "rpath", rFilePreamble + qtr("by (full) directory-path."),
                        QOptSqlArg::cmpOpsText());
    parser.addArg(&argRPath);

    QOptSqlArg argRSize("rs", "rsize", rFilePreamble + qtr("by filesize."),
                       QOptSqlArg::cmpOpsAllButLike() );
    argRSize.setIsByteSizeArg(true);
    parser.addArg(&argRSize);

    QOptSqlArg argRMtime("rm", "rmtime", rFilePreamble + qtr("by mtime."),
                       QOptSqlArg::cmpOpsAllButLike() );
    parser.addArg(&argRMtime);

    // TODO: argRHash (and others?)

    QOptArg argMaxReadFileLines("", "max-rfile-lines",
                            qtr("Display at most the first N lines for each "
                                "read file.")
                            );
    parser.addArg(&argMaxReadFileLines);

    QOptArg argRestoreRfiles("", "restore-rfiles",
                            qtr("Restore read files for the found commands at the system's "
                                "temporary directory."),
                             false
                            );
    parser.addArg(&argRestoreRfiles);

    QOptArg argRestoreRfilesAt("", "restore-rfiles-at",
                             qtr("Restore read files for the found commands at the given "
                                 "path.")
                             );
    parser.addArg(&argRestoreRfilesAt);

    QOptArg argRestoreRfileId("", "restore-rfile-id",
                             qtr("Restore the read file with the given id at the working directory. "
                                 "Please note that id's are not necessarily in "
                                 "an ascending order.")
                             );
    parser.addArg(&argRestoreRfileId);

    // ------------ cmd

    QOptSqlArg argCmdText("cmdtxt", "command-text", qtr("Query for commands with matching command-string."),
                        QOptSqlArg::cmpOpsText());
    parser.addArg(&argCmdText);

    QOptSqlArg argCmdCwd("cwd", "command-working-dir",
                         qtr("Query for commands with matching working-directory."),
                          QOptSqlArg::cmpOpsText());
    parser.addArg(&argCmdCwd);

    QOptSqlArg argCmdId("cmdid", "command-id", qtr("Query for commands with matching ids. "
                                                   "Please note that id's are not necessarily in "
                                                   "an ascending order."),
                        QOptSqlArg::cmpOpsAllButLike());
    parser.addArg(&argCmdId);

    QOptSqlArg argCmdEndDate("cmded", "command-end-date", qtr("Query for commands based on "
                                                              "the date (time) they finished."),
                        QOptSqlArg::cmpOpsAllButLike());
    parser.addArg(&argCmdEndDate);

    // ------------

    QOptSqlArg argShellSessionId("sid", "shell-session-id",
                                 qtr("Query for all commands with a given shell-session-id."),
                        QOptSqlArg::cmpOpsEqNe());
    parser.addArg(&argShellSessionId);

    const uint DEFAULT_wfilesMaxCount = 10;
    QOptArg argWfilesMaxCount("wfc", "wfiles-max-count",
                              qtr("Limit the number of rendered written files "
                                  "per command (default is %1)").arg(DEFAULT_wfilesMaxCount));
    parser.addArg(&argWfilesMaxCount);

    const uint DEFAULT_rfilesMaxCount = 10;
    QOptArg argRfilesMaxCount("rfc", "rfiles-max-count",
                              qtr("Limit the number of rendered read files "
                                  "per command (default is %1)").arg(DEFAULT_rfilesMaxCount));
    parser.addArg(&argRfilesMaxCount);

    QOptArg argOutputFile("o", "output",
                          qtr("Specify an output file where the report "
                              "is written to. Otherwise it is printed "
                              "to stdout"));
    parser.addArg(&argOutputFile);

    QOptArg argOutputFormat("", "output-format",
                            qtr("Specify the output format (human is default). "
                                "If 'html' is used, %1 must also be specified")
                            .arg(argOutputFile.name())); 

    const char* OUTPUT_FORMAT_HUMAN = "human";
    argOutputFormat.setAllowedOptions({OUTPUT_FORMAT_HUMAN, "json", "html"});
    parser.addArg(&argOutputFormat);

    QOptArg argStatCounts("", "stat-counts",
                          qtr("Specify the min. and max. number of entries "
                              "for the overall statistics (e.g. commands with most file modifications) "
                              "as a comma-separated list, e.g. '5,10' to display at least 5 but not more than"
                              "10 entries."));
    parser.addArg(&argStatCounts);


    // --------------------- End of Args -----------------------

    parser.parse(argc, argv);

    auto & trSnips = TrSnippets::instance();

    SqlQuery query;

    std::unique_ptr<CommandPrinter> cmdPrinter;
    if(argOutputFormat.wasParsed()){
        switch(argOutputFormat.getOptions(1).first()[1].toLatin1()){
        case 'u': cmdPrinter = std::unique_ptr<CommandPrinter>(new CommandPrinterHuman); break;
        case 's': cmdPrinter = std::unique_ptr<CommandPrinter>(new CommandPrinterJson);break;
        case 't': cmdPrinter = std::unique_ptr<CommandPrinter>(new CommandPrinterHtml);break;
        default: throw QExcProgramming("Bad output format:" + argOutputFormat.getOptions(1).first());
        }
    } else {
        cmdPrinter = std::unique_ptr<CommandPrinter>(new CommandPrinterHuman);
    }
    cmdPrinter->setQueryString(argvToQStr(argc, argv));

    cmdPrinter->setMaxCountWfiles(argWfilesMaxCount.getValue<uint>(DEFAULT_wfilesMaxCount));
    cmdPrinter->setMaxCountRfiles(argRfilesMaxCount.getValue<uint>(DEFAULT_rfilesMaxCount));
    {
        const auto statCounts = argStatCounts.getValuesByDelim<QVector<uint> >(",", {5,5}, 2,2);
        if(statCounts[0] > statCounts[1]){
            throw ExcOptArgParse(qtr("argument %1: min. cannot be greater than max. stat-count")
                                 .arg(argStatCounts.name()));
        }
        cmdPrinter->setMinCountOfStats(statCounts[0]);
        cmdPrinter->cmdStats().setMaxCountOfStats(statCounts[1]);
    }

    if(argOutputFile.wasParsed()){
        cmdPrinter->outputFile().setFileName(argOutputFile.getValue<QString>());
    } else {
        if(dynamic_cast<CommandPrinterHtml*>(cmdPrinter.get()) != nullptr){
            QIErr() << qtr("For html-reports, please specify an output file "
                           "(arg %1).").arg(argOutputFile.name());
            cpp_exit(1);
        }
        cmdPrinter->outputFile().open(stdout, QFile::OpenModeFlag::WriteOnly);
    }


    if(argMaxReadFileLines.wasParsed()){
        auto cmdPrinterHuman = dynamic_cast<CommandPrinterHuman*>(cmdPrinter.get());
        if(cmdPrinterHuman == nullptr){
            QIErr() << qtr("Argument %1 is only allowed with output-format '%2'")
                       .arg(argMaxReadFileLines.name(), OUTPUT_FORMAT_HUMAN);
            cpp_exit(1);
        }
        cmdPrinterHuman->setMaxCountOfReadFileLines(int(argMaxReadFileLines.getValue<uint>(5)));
    }
    cmdPrinter->setRestoreReadFiles(argRestoreRfiles.wasParsed() || argRestoreRfilesAt.wasParsed());

    if(argRestoreRfilesAt.wasParsed()){
        QDir restoreDir(argRestoreRfilesAt.getValue<QString>());
        if(! restoreDir.exists()){
            QIErr() << qtr("Restore directory %1 does not exist.").arg(restoreDir.absolutePath());
            cpp_exit(1);
        }
        restoreDir.setPath(restoreDir.absolutePath() + QDir::separator() + trSnips.shournalRestore);
        cmdPrinter->setRestoreDir(restoreDir);
    }  

    if(argRestoreRfileId.wasParsed()){
        restoreSingleReadFile(argRestoreRfileId);
    }

    QueryColumns & cols = QueryColumns::instance();

    addSimpleSqlArgToQueryIfParsed<QString>(query, argWName, cols.wFile_name);
    addSimpleSqlArgToQueryIfParsed<QString>(query, argWPath, cols.wFile_path);
    addBytesizeSqlArgToQueryIfParsed(query, argWSize, cols.wFile_size);
    if(argWHash.wasParsed()){
        HashValue hashVal(argWHash.getValue<uint64_t>());
        query.addWithAnd(cols.wFile_hash, db_conversions::fromHashValue(hashVal),
                         argWHash.parsedOperator());
    }
    addVariantSqlArgToQueryIfParsed<QDateTime>(query, argWMtime, cols.wfile_mtime);


    addSimpleSqlArgToQueryIfParsed<QString>(query, argRName, cols.rFile_name);
    addSimpleSqlArgToQueryIfParsed<QString>(query, argRPath, cols.rFile_path);
    addBytesizeSqlArgToQueryIfParsed(query, argRSize, cols.rFile_size);
    addVariantSqlArgToQueryIfParsed<QDateTime>(query, argRMtime, cols.rFile_mtime);

    addVariantSqlArgToQueryIfParsed<qint64>(query, argCmdId, cols.cmd_id);
    addSimpleSqlArgToQueryIfParsed<QString>(query, argCmdText, cols.cmd_txt);
    addSimpleSqlArgToQueryIfParsed<QString>(query, argCmdCwd, cols.cmd_workingDir);
    addVariantSqlArgToQueryIfParsed<QDateTime>(query, argCmdEndDate, cols.cmd_endtime);

    if(argShellSessionId.wasParsed()){
        auto shellSessionUUID = QByteArray::fromBase64(argShellSessionId.getValue<QByteArray>());
        query.addWithAnd(cols.session_id, shellSessionUUID,
                         argShellSessionId.parsedOperator());
    }

    if(argWFile.wasParsed()){
        if(argTakeFromWFile.wasParsed()){
            bool mtime=false;
            bool hash=false;
            bool size=false;
            for(auto opt : argTakeFromWFile.getOptions()){
                switch(opt[0].toLatin1()){
                case 'm': mtime = true; break;
                case 'h': hash = true; break;
                case 's': size = true; break;
                default: throw QExcProgramming("Bad argTakeFromWFile option:" + opt);
                }
            }
            file_query_helper::addWrittenFile(query, argWFile.getValue<QString>(),
                                          mtime, hash, size);
        } else {
            file_query_helper::addWrittenFileSmart(query, argWFile.getValue<QString>());
        }

    }


    // we always display commands in startDate-order, however,
    // to allow for a performant history query (where the last
    // N entries are queried) we traverse the result-set from
    // end -> reverseResultIter = true AND query.ascending = false.
    bool reverseResultIter=false;

    // argHistory *must* be last, in case of an otherwise empty
    // query, accept all (where 1).
    if(argHistory.wasParsed()){
        reverseResultIter = true;
        query.setAscending(false);
        query.setLimit(static_cast<int>(argHistory.getValue<uint>()));
        if(query.isEmpty()){
            // accept everything
            query.setQuery(" 1 ");
        }
    }

    if( parser.rest().len != 0){
        QIErr() << qtr("Invalid parameters passed: «%1».\n"
                       "Show help with --query --help").
                   arg(argvToQStr(parser.rest().len, parser.rest().argv));
        cpp_exit(1);
    }

    if(query.isEmpty()){
        QIErr() << qtr("No target fields given (empty query).");
        cpp_exit(1);
    }

    queryCmdPrintAndExit(cmdPrinter, query, reverseResultIter);
}


