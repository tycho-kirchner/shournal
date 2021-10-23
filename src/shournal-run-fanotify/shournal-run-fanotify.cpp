#include <csignal>
#include <fcntl.h>
#include <cassert>

#include <QCoreApplication>
#include <QVarLengthArray>


#include "qoptargparse.h"
#include "qoptvarlenarg.h"
#include "excoptargparse.h"
#include "os.h"
#include "excos.h"
#include "filewatcher_fan.h"
#include "msenter.h"
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
#include "mount_controller.h"
#include "orig_mountspace_process.h"
#include "cpp_exit.h"
#include "db_connection.h"
#include "storedfiles.h"
#include "socket_message.h"

using fdcommunication::SocketCommunication;
using socket_message::E_SocketMsg;

namespace  {

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
void callFilewatcherSafe(FileWatcher& fwatcher){

    try {
        fwatcher.run();
    } catch (const os::ExcOs & ex) {
        logCritical << qtr("Sorry, need to close: ") << ex.what();
    } catch(const ExcMountCtrl & ex){
        logCritical << qtr("mount failed: ") << ex.descrip();
    }
    if(fwatcher.sockFd() != -1){
        SocketCommunication fdCom;
        fdCom.setSockFd(fwatcher.sockFd());
        fdCom.sendMsg(int(E_SocketMsg::SETUP_FAIL));
    }
    cpp_exit(1);
}

} //  namespace


int shournal_run_main(int argc, char *argv[])
{    
    // Since we are waiting for other processes to finish, ignore typical
    // signals. Note that the below dummy-function is *not* equivalent to SIG_IGN:
    // while the dummy is reset, SIG_IGN is inherited on execve...
    for(int s : os::catchableTermSignals()){
        os::signal(s, [](int){});
    }
    // Using app::SHOURNAL for several common paths (database, config) used
    // by QStandardPaths but app::CURRENT_NAME for others (log-filename)
    app::setupNameAndVersion(app::SHOURNAL_RUN_FANOTIFY);

    if(! translation::init()){
        logWarning << "Failed to initialize translation";
    }
    if(os::geteuid() != 0){
        QIErr() << qtr("%1 seems to lack the suid-bit (SETUID) for root. You can correct "
                       "that by\n"
                       "chown root %1 && chmod u+s %1").arg(app::CURRENT_NAME);
        // but continue to allow for e.g. msenter-orig to exec the process anyway...
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
    parser.setHelpIntroduction(qtr("Observation backend for <%1>. "
                                   "Not meant to be called by users directly, "
                                   "you would rather call %1 without trailing '-run'."
                                      ).arg(app::SHOURNAL) + "\n");
    QOptArg argVersion("v", "version", qtr("Display version"), false);
    parser.addArg(&argVersion);

    // for communication with the shell-integration:
    QOptArg argSocketFd("", "socket-fd", "" );
    argSocketFd.setInternalOnly(true);
    parser.addArg(&argSocketFd);

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

    // Some variables like $TMPDIR are cleared in setuid-programs.
    // We allow them to be passed within argv and apply them during execve.
    // *after* setuid to original user.
    // see e.g. http://lists.gnu.org/archive/html/bug-glibc/2003-08/msg00076.html
    QOptVarLenArg argEnv("", "env", qtr("Specify an arbitrary number of environment variables. "
                                        "The first entry (integer) specifies the count "
                                        "of all the latter. The last entry is used internally and "
                                        "must be the string 'SHOURNAL_DUMMY_NULL=1'"));
    parser.addArg(&argEnv);

    // TODO: currently only interface compatibility.
    // We could indeed fork as well, at least in the exec-case.
    QOptArg argFork("", "fork",
                         qtr("NOT USED"), false);
    argFork.setInternalOnly(true);
    parser.addArg(&argFork);

    QOptArg argTmpDir("", "tmpdir",
                      qtr("Use the given TMPDIR (for non security-relevant stuff). "
                          "As a setuid binary some variables are cleared from "
                          "the environment (see also man 8 ld.so)"));
    // We expect to be called by the binary 'shournal' or the
    // shell integration, which both pass $TMPDIR using --tmpdir, so no need
    // to make it public.
    argTmpDir.setInternalOnly(true);
    parser.addArg(&argTmpDir);

    QOptArg argMsenter("", "msenter", qtr("<pid>. Must be passed along with '%1'. Execute the "
                                          "given command in an existing mountspace which was "
                                          "previously created via %2 %1")
                            .arg(argExec.name(), app::CURRENT_NAME));
    argMsenter.addRequiredArg(&argExec);
    parser.addArg(&argMsenter);

    QOptArg argMsenterOrig("", "msenter-orig-mountspace",
                           qtr("Must be passed along with '%1'. Execute the "
                               "given command in the 'original' mount-namespace "
                               "created the first time %2 observed a process.")
                            .arg(argExec.name(), app::CURRENT_NAME), false);
    argMsenterOrig.addRequiredArg(&argExec);
    parser.addArg(&argMsenterOrig);

    QOptArg argVerbosity("", "verbosity", qtr("How much shall be printed to stderr. Note that "
                                              "for 'dbg' shournal-run must not be a 'Release'-build."));
    argVerbosity.setAllowedOptions({"dbg",
                                #if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
                                    "info",
                                #endif
                                    "warning", "critical"});
    parser.addArg(&argVerbosity);

    QOptArg argShellSessionUUID("", "shell-session-uuid", qtr("uuid as base64-encoded string"));
    argShellSessionUUID.setInternalOnly(true);
    parser.addArg(&argShellSessionUUID);

    QOptArg argNoDb("", "no-db", qtr("For debug purposes: do not write to"
                                     "database after event processing"), false);
    parser.addArg(&argNoDb);

    QOptArg argPrintSummary("", "print-summary",
                         qtr("Print a short summary after "
                             "event processing finished."), false);
    parser.addArg(&argPrintSummary);

    try {
        parser.parse(argc, argv);

        if(argVerbosity.wasParsed()){
            QByteArray verbosity = argVerbosity.getOptions(1).first().toLocal8Bit();
            logger::setVerbosityLevel(verbosity.constData());
        } else {
            logger::setVerbosityLevel(QtMsgType::QtWarningMsg);
        }

        char** cmdEnv;
        if(argEnv.wasParsed()){
            cmdEnv = argEnv.vals().argv;
            auto envArgc = argEnv.vals().len;
            assert(strcmp(cmdEnv[envArgc-1], "SHOURNAL_DUMMY_NULL=1" ) == 0);
            cmdEnv[envArgc-1] = nullptr;
        } else {
            cmdEnv = environ;
        }

        char* cmdFilename = nullptr;
        char** cmdArgv = nullptr;
        if(argExec.wasParsed()){
            cmdArgv = parser.rest().argv;
            if(argExecFilename.wasParsed()){
                cmdFilename = argExecFilename.vals().argv[0];
            } else {
                cmdFilename = parser.rest().argv[0];
            }
        }        

        // Don't waste time, msenter has to run as early as possible
        if(argMsenter.wasParsed()){
            msenter::run(argMsenter.getValue<pid_t>(), cmdFilename, cmdArgv, cmdEnv);
        }

        if(argExec.wasParsed() &&
                argSocketFd.wasParsed() ) {
            QIErr() << qtr("%1 and %2 are mutually exclusive").arg(argExec.name(), argSocketFd.name());
            cpp_exit(1);
        }

        if(argVersion.wasParsed()){
            QOut() << app::CURRENT_NAME << qtr(" version ") << app::version().toString() << "\n";
            cpp_exit(0);
        }

        // has to be before argExec
        if(argMsenterOrig.wasParsed()){
            // do not crash here if called from shell-integration, if
            // we forgot to make shournal suid...
            bool weAreAnotheUser = os::geteuid() != os::getuid();
            if(weAreAnotheUser) os::seteuid(os::getuid());
            logger::enableLogToFile(app::CURRENT_NAME);
            if(weAreAnotheUser) os::seteuid(0);
            orig_mountspace_process::msenterOrig(cmdFilename, cmdArgv, cmdEnv);
            // never get here
        }

        // ------------------------------  //

        // In file observation mode (or invalid commandline-input)

        FileWatcher fwatcher;
        fwatcher.setCommandEnvp(cmdEnv);
        if(argExecFilename.wasParsed()){
            // fwatcher command-filename must otherwise be null, to allow
            // for correct storing of command in db (no duplicate first arg
            // if not necessary!)
            fwatcher.setCommandFilename(argExecFilename.vals().argv[0]);
        }
        if(argTmpDir.wasParsed()){
            fwatcher.setTmpDir(argTmpDir.getValue<QByteArray>());
        }

        fwatcher.setStoreToDatabase(! argNoDb.wasParsed());
        fwatcher.setPrintSummary(argPrintSummary.wasParsed());

        // load settings as real user (this is a setuid-program)
        // also enable logging already, *before* possibly unsharing the
        // mount-namespace.
        os::seteuid(os::getuid());
        try {
            logger::enableLogToFile(app::CURRENT_NAME);
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
        os::seteuid(0);

        if(argShellSessionUUID.wasParsed()){
            fwatcher.setShellSessionUUID(
                        QByteArray::fromBase64(argShellSessionUUID.getValue<QByteArray>()));
        }

        if(argSocketFd.wasParsed()){
            int socketFd = argSocketFd.getValue<int>(-1);
            os::setFdDescriptorFlags(socketFd, FD_CLOEXEC);
            fwatcher.setSockFd(socketFd);
            callFilewatcherSafe(fwatcher);
        }

        if(argExec.wasParsed()){
            assert(!argMsenterOrig.wasParsed());
            auto externCmd = parser.rest();
            fwatcher.setArgv(externCmd.argv, externCmd.len);
            callFilewatcherSafe(fwatcher);
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



