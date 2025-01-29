#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <csignal>

#include <QtCore>
#include <QCoreApplication>
#include <QMimeDatabase>
#include <cassert>

#include "qoptargparse.h"
#include "excoptargparse.h"
#include "excos.h"
#include "logger.h"

#include "os.h"
#include "exccfg.h"
#include "cpp_exit.h"
#include "settings.h"
#include "db_connection.h"
#include "util.h"
#include "cleanupresource.h"
#include "qoutstream.h"
#include "util.h"
#include "translation.h"
#include "app.h"
#include "qsqlquerythrow.h"
#include "argcontrol_dbquery.h"
#include "argcontrol_dbdelete.h"
#include "qexcdatabase.h"
#include "conversions.h"
#include "console_dialog.h"
#include "qfilethrow.h"
#include "shournal_run_common.h"

using namespace shournal_run_common;

/// Uncaught exception handler
void onterminate() {
    try {
        auto unknown = std::current_exception();
        if (unknown) {
            std::rethrow_exception(unknown);
        }
    } catch (const std::exception& e) {
        logCritical << e.what() << "\n";
    } catch (...) {
        logCritical << "unknown exception occurred\n";
    }
}

[[noreturn]]
void execShournalRun(const QByteArray& backendFilename,
                     QOptArg::RawValues_t &cargs, bool withinOrigMountspace,
                     QVarLengthArray<QOptArg*> forwardArgs);


int shournal_main(int argc, char *argv[])
{
    app::setupNameAndVersion(app::SHOURNAL);

    if(! translation::init()){
        QIErr() << "Failed to initialize translation";
    }

    logger::setup(app::CURRENT_NAME);

    std::set_terminate(onterminate);
    if(! shournal_common_init()){
        logCritical << qtr("Fatal error: failed to initialize custom Qt conversion functions");
        cpp_exit(1);
    }

    // ignore first arg (command to this app)
    --argc;
    ++argv;

    auto & sets = Settings::instance();

    QOptArgParse parser;
    parser.setHelpIntroduction(qtr("Launch a command and recursively observe in specific "
                                   "directories which files "
                                   "were modified by that process and its children. ") + "\n");
    QOptArg argVersion("v", "version", qtr("Display version"), false);
    parser.addArg(&argVersion);

    QOptArg argExec("e", "exec",
                    qtr("Execute and observe the passed program "
                        "and its arguments (this argument has to be last). "
                        "All further parameters starting with a minus "
                        "are considered options for the shournal-run* backend until "
                        "double dash -- or the first command "
                        "(not starting with a minus) occurs, e.g.\n"
                        "shournal -e --print-summary echo foobar\n"
                        "-> --print-summary is an argument for shournal-run.\n"
                        "shournal --exec-filename /bin/bash -e -- -bash\n"
                        "on the other hand can be used for commands starting "
                        "with a dash (e.g. login-shells)."), false);
    argExec.setFinalizeFlag(true);
    parser.addArg(&argExec);

    QOptArg argExecFilename("", "exec-filename", qtr("This is an advanced option. "
                                                     "In most cases the first argument of a "
                                                     "program is the program name. For "
                                                     "example for login-shells this does "
                                                     "not have to be the case. If this "
                                                     "argument is provided, that filename "
                                                     "is used instead of argv[0]"));
    parser.addArg(&argExecFilename);
    argExecFilename.addRequiredArg(&argExec);


    QOptArg argBackend("", "backend-filename",
                       qtr("When executing a command (option %1) use "
                           "the given filename as observation backend-command")
                           .arg(argExec.name()));
    parser.addArg(&argBackend);
    argBackend.addRequiredArg(&argExec);



    QOptArg argMsenterOrig("", "msenter-orig-mountspace",
                           qtr("Must be passed along with '%1'. Execute the "
                               "given command in the 'original' mount-namespace "
                               "created the first time %2 observed a process.")
                            .arg(argExec.name(), app::SHOURNAL_RUN), false);
    argMsenterOrig.setInternalOnly(true);
    argMsenterOrig.addRequiredArg(&argExec);
    parser.addArg(&argMsenterOrig);

    QOptArg argEditCfg("c", "edit-cfg", qtr("Edit the config-file at %1 "
                                            "with your favourite text-editor:\n"
                                            "export EDITOR='...'").arg(sets.cfgFilepath()),
                                             false);
    parser.addArg(&argEditCfg);

    QOptArg argQuery("q", "query", qtr("Query %1's database for activities. Type "
                                       "--query --help for details.").arg(app::SHOURNAL), false);
    argQuery.setFinalizeFlag(true);
    parser.addArg(&argQuery);

    QOptArg argDelete("", "delete", qtr("delete (parts of) %1's command history from the"
                                         " database. Type "
                                       "--delete --help for details.").arg(app::SHOURNAL), false);
    argDelete.setFinalizeFlag(true);
    parser.addArg(&argDelete);

    QOptArg argPrintMime("", "print-mime", qtr("Print the mimetpye of an existing file(name) "
                                               "which can be used in shournal's config-"
                                               "file for setting file-event-rules."));
    parser.addArg(&argPrintMime);


    QOptArg argVerbosity("", "verbosity", qtr("How much shall be printed to stderr. Note that "
                                              "for 'dbg' shournal must not be a 'Release'-build, "
                                              "dbug-messages are lost in Release-mode."));
    argVerbosity.setAllowedOptions(app::VERBOSITIES);
    parser.addArg(&argVerbosity);

    QOptArg argValidateSettings("", "validate-settings",
                                qtr("If the settings-file is well formed, "
                                    "return 0, else print an error and return "
                                    "a nonzero value"), false);
    parser.addArg(&argValidateSettings);

    QOptArg argLsOurPaths("", "ls-paths",
                                qtr("Print shournal's application paths (database-dir, etc.)"), false);

    parser.addArg(&argLsOurPaths);

    auto argCfgDir = mkarg_cfgdir();
    parser.addArg(&argCfgDir);
    auto argDataDir = mkarg_datadir();
    parser.addArg(&argDataDir);

    // Forward these to shournal-run:
    QVarLengthArray<QOptArg*> forwardArgs = {&argVerbosity, &argExecFilename, &argCfgDir,
                                           &argDataDir};

    try {
        parser.parse(argc, argv);
        auto & sets = Settings::instance();
        if(argCfgDir.wasParsed()){
            sets.setUserCfgDir(argCfgDir.getValue<QString>());
        }
        if(argDataDir.wasParsed()){
            sets.setUserDataDir(argDataDir.getValue<QString>());
        }

        if(argVerbosity.wasParsed()){
            QByteArray verbosity = argVerbosity.getOptions(1).first().toLocal8Bit();
            logger::setVerbosityLevel(verbosity.constData());
        } else {
            logger::setVerbosityLevel(QtMsgType::QtWarningMsg);
        }

        if(argExec.wasParsed()){
            auto backendFilename = argBackend.getValue<QString>();
            if(backendFilename.isEmpty()){
                backendFilename = sets.chooseShournalRunBackend();
                if(backendFilename.isEmpty()){
                    QIErr() << qtr("No backend-filename given and no valid "
                                   "backend found - exiting...");
                    cpp_exit(1);
                }
            }
            execShournalRun(backendFilename.toLocal8Bit(),
                            parser.rest(), argMsenterOrig.wasParsed(),
                            forwardArgs);
        }

        if(argVersion.wasParsed()){
            QOut() << app::SHOURNAL << qtr(" version ") << app::version().toString() << "\n";
            cpp_exit(0);
        }

        logger::enableLogToFile(app::SHOURNAL);

        sets.load();
        if(argValidateSettings.wasParsed()){
            cpp_exit(0);
        }

        if(argQuery.wasParsed()){
            argcontol_dbquery::parse(parser.rest().len, parser.rest().argv);
            // never get here
        }

        if(argDelete.wasParsed()){
            argcontrol_dbdelete::parse(parser.rest().len, parser.rest().argv);
            // never get here
        }

        if(argEditCfg.wasParsed()){
            int ret = console_dialog::openFileInExternalEditor(sets.cfgFilepath());
            cpp_exit(ret);
        }

        if(argPrintMime.wasParsed()){
            QFileThrow f(argPrintMime.getValue<QString>());
            f.open(QFile::OpenModeFlag::ReadOnly);
            auto mtype = QMimeDatabase().mimeTypeForData(&f);
            QOut() << mtype.name() << "\n";
            cpp_exit(0);
        }

        if(argLsOurPaths.wasParsed()){
            QOut() << qtr("Database directory: ") << db_connection::getDatabaseDir() << "\n"
                   << qtr("Configuration directory: ")
                           << splitAbsPath(sets.cfgFilepath()).first << "\n"
                   << qtr("Cache directory (log-files): ") << logger::logDir() << "\n" ;
            cpp_exit(0);
        }


        if(parser.rest().len != 0){
            QIErr() << qtr("Invalid parameters passed: %1.\n"
                           "Show help with --help").
                       arg( argvToQStr(parser.rest().len, parser.rest().argv));
             cpp_exit(1);
        }
        QIErr() << "No action specified";

    } catch (const ExcOptArgParse & ex) {
        QIErr() << qtr("Commandline seems to be erroneous:")
                << ex.descrip();
    } catch (const ExcConversion & ex) {
        QIErr() << ex.descrip();
    }
    catch(const qsimplecfg::ExcCfg & ex){
        QIErr() << qtr("Failed to load config file: ") << ex.descrip();
    } catch (const QExcIo& ex){
        QIErr() << qtr("IO-operation failed: ") << ex.descrip();
    } catch (const os::ExcOs& ex){
        QIErr() << ex.what();
    }
    cpp_exit(1);
}



void execShournalRun(const QByteArray& backendFilename,
                     QOptArg::RawValues_t& cargs , bool withinOrigMountspace,
                     QVarLengthArray<QOptArg*> forwardArgs){
    // setuid-programs change some parts of the environment for security reasons.
    // Therefor, pass the environment via argv and apply it later (after setuid
    // to original user)

    QVarLengthArray<const char*, 8192> args;
    args.push_back(backendFilename);
    if(withinOrigMountspace){
        args.push_back("--msenter-orig-mountspace");
    }

    QByteArray verbosityStr;
    QVarLengthArray<QByteArray> forwardArgsBuf;
    for(QOptArg* a : forwardArgs){
        if(a->wasParsed()){
            // we need another buffer for the char* args array.
            forwardArgsBuf.push_back(a->name().toLocal8Bit());
            args.push_back(forwardArgsBuf.last());
            args.push_back(a->vals().argv[0]);
        }
    }

    const char* tmpdir = getenv("TMPDIR");
    if(tmpdir != nullptr){
        args.push_back("--tmpdir");
        args.push_back(tmpdir);
    }

    args.push_back("--env");
    // first value after --env is its size, which we don't know yet.
    args.push_back("DUMMY");
    int envSizeIdx = args.size() -1;
    for (char **env = environ; *env != nullptr; env++) {
        args.push_back(*env);
    }
    // optimization in shournal-run...
    args.push_back("SHOURNAL_DUMMY_NULL=1");
    std::string envSize = std::to_string(args.size() - envSizeIdx - 1);
    args[envSizeIdx] = envSize.c_str();


    // As long as arguments start with a minus those
    // are passed as options to the shournal-run backend, e.g.
    // shournal-run -e --no-db --print-summary echo ok
    int execIdx = -1;
    for(int i=0; i < cargs.len; i++){
        if(execIdx != -1){
            args.push_back(cargs.argv[i]);
            continue;
        }
        if(strcmp(cargs.argv[i], "--") == 0){
            // option terminator -> all further options
            // will be directly passed to our backend.
            execIdx = args.size();
            args.push_back("--exec");
        } else if(cargs.argv[i][0] == '-'){
            // option for shournal-run
            args.push_back(cargs.argv[i]);
        } else {
            // first non-option.
            execIdx = args.size();
            args.push_back("--exec");
            args.push_back(cargs.argv[i]);
        }
    }
    if(execIdx == -1 || execIdx == args.size()-1){
        QIErr() << qtr("No executable found after parsing the commandline. "
                       "Note that for commands starting with dashes (e.g. login-shells) "
                       "--exec has to be terminated by double-dash --\n"
                       "Current arguments:");
        for(const auto& arg : args){
            QErr() << arg << " ";
        }
        QErr() << "\n";
        cpp_exit(1);
    }

    args.push_back(nullptr);
    os::exec(args);
}

int main(int argc, char *argv[])
{
    try {
        shournal_main(argc, argv);
    } catch (const ExcCppExit& e) {
        return e.ret();
    }
}

