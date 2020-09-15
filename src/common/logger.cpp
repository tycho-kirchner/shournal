#include <cassert>
#include <QDateTime>
#include <QtGlobal>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <utility>

#include "logger.h"
#include "qoutstream.h"
#include "app.h"
#include "exccommon.h"
#include "os.h"
#include "osutil.h"
#include "staticinitializer.h"
#include "cflock.h"



namespace  {
QString g_logPreamble;
const QtMsgType DEFAULT_VERBOSITY = QtMsgType::QtWarningMsg;
QtMsgType g_verbosityLvl = DEFAULT_VERBOSITY;
int g_verbosityLvlOrdinal=logger::msgTypeToOrdinal(DEFAULT_VERBOSITY);
pid_t g_pid;




void messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &msg)
{
    int typeOrdinal = logger::msgTypeToOrdinal(msgType);

#ifndef NDEBUG
    if (msgType == QtDebugMsg) {
        if(typeOrdinal >= g_verbosityLvlOrdinal){
            QErr() << g_logPreamble << " Dbg: "
                 << "(" << QFileInfo(context.file).fileName() <<":" << context.line << ") "
                 << " pid " << g_pid << ": "  << msg << '\n' ;
        }
        // Don't log debug messages to file
        return;
    }
#else
    Q_UNUSED(context)
#endif

    const QString dateTime = QDateTime::currentDateTime().toString(
                "yyyy-MM-dd HH:mm:ss");
    QString msgTypeStr = logger::msgTypeToStr(msgType);

    if(typeOrdinal >= g_verbosityLvlOrdinal){
        QErr() << g_logPreamble << " "<<dateTime<<' '<< msgTypeStr<<": "<<msg<< "\n";
    }
    if(logger::getLogRotate().file().isOpen()){
        logger::getLogRotate().stream() <<dateTime<<' '<< msgTypeStr
                                       << " pid " << g_pid << ": " <<msg << "\n";
    }
}

} // namespace




/// @param preamble: printed before every log message
void logger::setup(const QString& preamble)
{
    g_logPreamble = preamble;
    g_pid = getpid();
    qInstallMessageHandler(messageHandler);

}


void logger::enableLogToFile(const QString& filename)
{
    QDir d(logDir());
    if( ! d.mkpath(logDir())){
        throw QExcCommon(QString("Failed to create %1").arg(logDir()));
    }
    const QString path = logDir() + "/log_" + filename;
    getLogRotate().setFullpath(path);
    getLogRotate().setup();
}



void logger::disableLogToFile()
{
    getLogRotate().cleanup();
}


void logger::setVerbosityLevel(QtMsgType lvl)
{
    g_verbosityLvl = lvl;
    g_verbosityLvlOrdinal = msgTypeToOrdinal(lvl);
}

void logger::setVerbosityLevel(const char *str)
{
    setVerbosityLevel(strToMsgType(str));
}


QtMsgType logger::getVerbosityLevel()
{
    return g_verbosityLvl;
}



logger::LogRotate &logger::getLogRotate()
{
    static logger::LogRotate logRotate;
    return logRotate;
}



/// call enableLogToFile first
const QString& logger::logDir()
{
    static const QString logDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return logDir;
}


const char *logger::msgTypeToStr(QtMsgType msgType)
{
    switch (msgType) {
    case QtDebugMsg: return "dbg";
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    case QtInfoMsg     : return "info";
#endif
    case QtWarningMsg  : return "warning";
    case QtCriticalMsg : return "critical";
    case QtFatalMsg    : return "fatal";
    }
    static StaticInitializer initOnFirstCall( [&msgType](){
        logWarning << "msgTypeToStr" << "unknown messagetype" << msgType;
    });
    return "warning";
}




/// Unfortunately qt messagetype are not really in a meaningful order - do it ourselves.
int logger::msgTypeToOrdinal(QtMsgType msgType)
{
    switch (msgType) {
    case QtDebugMsg: return 0;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    case QtInfoMsg     : return 1;
#endif
    case QtWarningMsg  : return 2;
    case QtCriticalMsg : return 3;
    case QtFatalMsg    : return 4;
    }
    static StaticInitializer initOnFirstCall( [&msgType](){
        logWarning << "msgTypeToOrdinal" << "unknown messagetype" << msgType;
    });
    return 2;
}



/// str is epected to be valid!
QtMsgType logger::strToMsgType(const char *str)
{
    switch (str[0]) {
    case 'd': return QtMsgType::QtDebugMsg;
    case 'i':
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
         return QtMsgType::QtInfoMsg;
#else
         return QtMsgType::QtWarningMsg;
#endif
    case 'w': return QtMsgType::QtWarningMsg;
    case 'c': return QtMsgType::QtCriticalMsg;
    case 'f': return QtMsgType::QtFatalMsg;
    default: break;
    }
    static StaticInitializer initOnFirstCall( [&str](){
        logWarning << "strToMsgType" << "unknown messagetype" << str
                   << "using default";
    });
    return DEFAULT_VERBOSITY;
}




logger::LogRotate::LogRotate(QString  fullpath)
    : m_fullpath(std::move(fullpath))
{
}

QTextStream &logger::LogRotate::stream()
{
    return m_stream;
}


const QFile& logger::LogRotate::file() const
{
    return m_file;
}


void logger::LogRotate::openLogfileOrThrow()
{
    if(! m_file.open(QFile::OpenModeFlag::Append | QIODevice::Text)){
        throw QExcIo(qtr("Failed to open logile at %1 - %2").arg(m_fullpath,
                     m_file.errorString()));
    }
}

/// Open the log file in append mode, rotate logfiles race-free, if too big.
/// @throws ExcOs, QExcIo
void logger::LogRotate::setup()
{
    assert(! m_fullpath.isEmpty());
    m_file.setFileName(m_fullpath);

    openLogfileOrThrow();

    if(os::fstat(m_file.handle()).st_size > 50000){
        // race condition - make sure to only rename once:
         CFlock l(m_file.handle());
         l.lockExclusive();
         // renamed already (by another process)?
         if(! osutil::findPathOfFd<QByteArray>(m_file.handle()).endsWith("_old")){
             const std::string path = m_fullpath.toStdString();
             os::rename(path, path + "_old");
         }
         l.unlock();
         m_file.close();
         // open or create the new logfile:
         openLogfileOrThrow();
    }
    m_stream.setDevice(&m_file);
}

void logger::LogRotate::cleanup()
{
    m_file.close();
}


void logger::LogRotate::setFullpath(const QString &p)
{
    m_fullpath = p;
}








