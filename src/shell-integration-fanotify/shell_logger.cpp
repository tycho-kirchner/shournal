#include <stdarg.h>

#include <QtGlobal>
#include <QDateTime>
#include <QFileInfo>

#include "app.h"
#include "shell_logger.h"
#include "qoutstream.h"
#include "shell_globals.h"
#include "fdcommunication.h"
#include "logger.h"
#include "socket_message.h"

using socket_message::E_SocketMsg;

namespace {

struct ShellLogState {
    QString logPreamble;
    QVarLengthArray<QByteArray, 8192> bufferedMessages;
};

ShellLogState& sLogState(){
    static ShellLogState s;
    return s;
}


void sendViaSock(QByteArray& msg){
    try {
        ShellGlobals::instance().shournalSocket.sendMsg({int(E_SocketMsg::LOG_MESSAGE),
                                              msg} );
    } catch (const os::ExcOs& e) {
        QIErr() << "Failed to send message via socket:" << e.what();
    }
}


void messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &msg)
{
    auto& g_shell = ShellGlobals::instance();
    int desiredVerbosity = logger::msgTypeToOrdinal(g_shell.verbosityLevel);
    int typeOrdinal = logger::msgTypeToOrdinal(msgType);

#ifndef NDEBUG
    if (msgType == QtDebugMsg) {
        if(typeOrdinal >= desiredVerbosity){
            QErr() << sLogState().logPreamble << " Dbg: "
                   << "(" << QFileInfo(context.file).fileName() <<":" << context.line << ") "
                   << "pid " << getpid() << ": "
                   << msg << '\n' ;
        }
        return;
    }
#else
    Q_UNUSED(context)
#endif

    QString msgTypeStr = logger::msgTypeToStr(msgType);

    const QString dateTime = QDateTime::currentDateTime().toString(
                "yyyy-MM-dd HH:mm:ss");

    if(typeOrdinal >= desiredVerbosity){
        QErr() << sLogState().logPreamble << " "      <<dateTime<<' '<< msgTypeStr<<": "<<msg<< "\n";
    }
     QByteArray msgArr =
             (QString(dateTime + ' ' + msgTypeStr + " pid %1" + ": " + msg)
              .arg(getpid())).toLocal8Bit();
     if(g_shell.watchState == E_WatchState::WITHIN_CMD){
         sendViaSock(msgArr);
     } else {
         if(sLogState().bufferedMessages.size() + 1 > sLogState().bufferedMessages.capacity()){
             QErr() << sLogState().logPreamble << " " << qtr("Too many log-messages could not be sent "
                                                   "to external %1-process, "
                                                   "so some will be lost (not logged to disk). "
                                                   "This is most likely a bug.").arg(app::SHOURNAL);
             sLogState().bufferedMessages.clear();
         }
         sLogState().bufferedMessages.push_back(msgArr);
     }

}

} // namespace

void shell_logger::setup()
{
    sLogState().logPreamble = QString(app::SHOURNAL) + " shell-integration";
    qInstallMessageHandler(messageHandler);
}



/// There is not always a socket to shournal open.
/// In that case, store them here until flushed
void shell_logger::flushBufferdMessages()
{
    for(QByteArray& msg : sLogState().bufferedMessages){
        sendViaSock(msg);
    }
    sLogState().bufferedMessages.clear();
}

/// Instead of logDebug use this function which works without any
/// complex initialization. Otherwise we might mess up global variables of
/// the attached program, e.g. qInstallMessageHandler or
/// QCoreApplication::setApplicationName ...
/// Note however that we remove *this shared libaray from LD_PRELOAD _before_
/// calling qInstallMessageHandler etc. (from within the shell integration scripts),
/// so we should be mostly safe.
/// In general it is probably a good idea to not use foreign complex functions
/// like qInstallMessageHandler from within the shell integration at all ..
void __shell_earlydbg(const char* file, int line, const char *format, ...)
{
    const char* verbosityValue = getenv(ENV_VARNAME_SHELL_VERBOSITY);
    if(verbosityValue == nullptr || strcmp(verbosityValue, "dbg") != 0){
        return;
    }
    fprintf(stderr, "shournal shell integration Dbg: (%s:%d) pid %d: ",
            file, line, os::getpid());

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}
