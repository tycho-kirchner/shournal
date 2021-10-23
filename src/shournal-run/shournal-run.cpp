#include <csignal>
#include <fcntl.h>
#include <cassert>

#include <QCoreApplication>
#include <QVarLengthArray>
#include <QDir>

#include "qoptargparse.h"
#include "qoptvarlenarg.h"
#include "excoptargparse.h"
#include "os.h"
#include "excos.h"
#include "filewatcher_shournalk.h"
#include "logger.h"
#include "fdcommunication.h"


#include "exccfg.h"
#include "settings.h"
#include "util.h"
#include "qoutstream.h"
#include "util.h"
#include "translation.h"
#include "app.h"
#include "qexcdatabase.h"
#include "cpp_exit.h"
#include "db_connection.h"
#include "storedfiles.h"
#include "socket_message.h"

using fdcommunication::SocketCommunication;
using socket_message::E_SocketMsg;



#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#include "os.h"
#include "osutil.h"
#include "fdentries.h"
#include "qoutstream.h"
#include "shournalk_ctrl.h"


/// Uncaught exception handler
static void onterminate() {
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

static void closeFds(){
    // close all file descriptors except stderr and
    // a potential integration test descriptor.
    auto keepFds = std::unordered_set<int>{2, app::findIntegrationTestFd()};
    for(int fd : osutil::FdEntries()){
        if(keepFds.find(fd) == keepFds.end()){
            os::close(fd);
        }
    }
}


static int shournal_run_main(int argc, char *argv[])
{
    // Since we are waiting for other processes to finish, ignore typical
    // signals. Note that the below dummy-function is *not* equivalent to SIG_IGN:
    // while the dummy is reset, SIG_IGN is inherited on execve...
    for(int s :  {SIGHUP, SIGINT, SIGPIPE}){
        os::signal(s, [](int){});
    }
    // Using app::SHOURNAL for several common paths (database, config) used
    // by QStandardPaths but app::CURRENT_NAME for others (log-filename)
    app::setupNameAndVersion(app::SHOURNAL_RUN);

    if(! translation::init()){
        logWarning << "Failed to initialize translation";
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
    QOptArgParse parser;
    parser.setHelpIntroduction(qtr("Observation backend for <%1> based "
                                   "on a custom kernel module."
                                      ).arg(app::SHOURNAL) + "\n");
    QOptArg argVersion("v", "version", qtr("Display version"), false);
    parser.addArg(&argVersion);

    QOptArg argPid("", "pid",
                         qtr("Mark the process with given pid for observation."));
    parser.addArg(&argPid);


    QOptArg argPrintFifopath("", "print-fifopath-for-pid",
                         qtr("Print the fifo path for a given pid "
                             "and exit. A fifo is "
                             "created, if argument %1 is given.")
                             .arg(argPid.name()));
    parser.addArg(&argPrintFifopath);

    QOptArg argFifoname("", "fifoname",
                         qtr("If arg %1 is also parsed, create "
                             "the fifo under the given filename").arg(argPid.name()));
    parser.addArg(&argFifoname);
    argFifoname.addRequiredArg(&argPid);

    QOptArg argTmpDir("", "tmpdir", "NOT USED");
    // Interface compatibility with shournal-run-fanotify.
    // Not being suid we can simply use $TMPDIR.
    argTmpDir.setInternalOnly(true);
    parser.addArg(&argTmpDir);

    QOptVarLenArg argEnv("", "env", "NOT USED");
    // Interface compatibility with shournal-run-fanotify.
    argEnv.setInternalOnly(true);
    parser.addArg(&argEnv);

    QOptArg argFork("", "fork",
                         qtr("Fork into background immediatly after marking "
                             "a pid."), false);
    parser.addArg(&argFork);

    QOptArg argCmdString("", "cmd-string",
                         qtr("Associate the recording of a process with the "
                             "given command string. Only used, if arg %1 "
                             "is given").arg(argPid.name()));
    argCmdString.addRequiredArg(&argPid);
    parser.addArg(&argCmdString);
    argPid.addRequiredArg(&argCmdString);

    QOptArg argCloseFds("", "close-fds",
                         qtr("Advanced option: closes file "
                             "descriptors except stderr."), false);
    parser.addArg(&argCloseFds);


    QOptArg argPrintSummary("", "print-summary",
                         qtr("Print a short summary after "
                             "event processing finished."), false);
    parser.addArg(&argPrintSummary);

    QOptArg argShournalkIsLoaded("", "shournalk-is-loaded",
                         qtr("If shournal's kernel module is loaded, "
                             "exit with zero, else nonzero"), false);
    parser.addArg(&argShournalkIsLoaded);


    QOptArg argExec("e", "exec", qtr("Execute and observe the passed program "
                                     "and its arguments (this argument has to be last)."),
                    false);
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

    QOptArg argVerbosity("", "verbosity",
                         qtr("How much shall be printed to stderr. Note that "
                             "for 'dbg' shournal-run must not be a 'Release'-build."));
    argVerbosity.setAllowedOptions(app::VERBOSITIES);
    parser.addArg(&argVerbosity);

    QOptArg argShellSessionUUID("", "shell-session-uuid", qtr("uuid as base64-encoded string"));
    argShellSessionUUID.setInternalOnly(true);
    parser.addArg(&argShellSessionUUID);

    QOptArg argMakeSessionUUID("", "make-session-uuid", qtr("print a unique uuid to stdout and "
                                                            "exit"), false);
    argMakeSessionUUID.setInternalOnly(true);
    parser.addArg(&argMakeSessionUUID);


    QOptArg argNoDb("", "no-db", qtr("For debug purposes: do not write to "
                                     "database after event processing"), false);
    parser.addArg(&argNoDb);

    try {
        parser.parse(argc, argv);

        if(argCloseFds.wasParsed()){
            // Do this early before we open fds ourselves.
            closeFds();
        }

        if(argVerbosity.wasParsed()){
            QByteArray verbosity = argVerbosity.getOptions(1).first().toLocal8Bit();
            logger::setVerbosityLevel(verbosity.constData());
        } else {
            logger::setVerbosityLevel(QtMsgType::QtWarningMsg);
        }

        if(argPrintFifopath.wasParsed()){
            QOut() << Filewatcher_shournalk::fifopathForPid(
                          argPrintFifopath.getValue<pid_t>()
                          ) << "\n";
            cpp_exit(0);
        }

        if(argShournalkIsLoaded.wasParsed()){
            cpp_exit(! shournalk_module_is_loaded());
        }

        if(argMakeSessionUUID.wasParsed()){
            bool madeSafe;
            auto uuid = make_uuid(&madeSafe);
            if(! madeSafe){
                logInfo << qtr("session uuid not created 'safe'. Is the uuidd-daemon running?");
            }
            QOut() << uuid.toBase64() << "\n";
            cpp_exit(0);
        }

        if(argExec.wasParsed() &&
                argPid.wasParsed() ) {
            QIErr() << qtr("%1 and %2 are mutually exclusive").arg(argExec.name(), argPid.name());
            cpp_exit(1);
        }

        if(argVersion.wasParsed()){
            QOut() << app::SHOURNAL_RUN << qtr(" version ") << app::version().toString() << "\n";
            cpp_exit(0);
        }

        try {
            logger::enableLogToFile(app::SHOURNAL_RUN);
            Settings::instance().load();
            StoredFiles::mkpath();
        } catch(const qsimplecfg::ExcCfg & ex){
            QIErr() << qtr("Failed to load config file: ") << ex.descrip();
            cpp_exit(1);
        } catch(const QExcDatabase & ex){
            QIErr() << qtr("Database-operation failed: ") << ex.descrip();
            cpp_exit(1);
        }  catch (const QExcIo& ex){
            logCritical << qtr("IO-operation failed: ") << ex.descrip();
            cpp_exit(1);
        } catch (const os::ExcOs& ex){
            logCritical << ex.what();
            cpp_exit(1);
        }

        Filewatcher_shournalk fwatcher;

        if(argExecFilename.wasParsed()){
            // fwatcher command-filename must otherwise be null, to allow
            // for correct storing of command in db (no duplicate first arg
            // if not necessary!)
            fwatcher.setCommandFilename(argExecFilename.vals().argv[0]);
        }

        fwatcher.setForkIntoBackground(argFork.wasParsed());
        fwatcher.setPrintSummary(argPrintSummary.wasParsed());
        fwatcher.setStoreToDatabase(! argNoDb.wasParsed());

        if(argShellSessionUUID.wasParsed()){
            fwatcher.setShellSessionUUID(
                        QByteArray::fromBase64(argShellSessionUUID.getValue<QByteArray>()));
        }
        if(argExec.wasParsed()){
            auto externCmd = parser.rest();
            fwatcher.setArgv(externCmd.argv, externCmd.len);
            fwatcher.run();
        }
        if(argPid.wasParsed()){
            fwatcher.setPid(argPid.getValue<pid_t>(INVALID_PID));
            fwatcher.setFifoname(argFifoname.getValue<QByteArray>(
                                 Filewatcher_shournalk::fifopathForPid(getpid())
                                     ));
            assert(argCmdString.wasParsed());
            fwatcher.setCmdString(argCmdString.getValue<QString>());
            fwatcher.run();
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
    }
    cpp_exit(1);


}

int main(int argc, char *argv[])
{
    try {
        shournal_run_main(argc, argv);
    } catch (const ExcCppExit& e) {
        return e.ret();
    }
}



