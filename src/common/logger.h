#pragma once

#include <QtGlobal>
#include <QDebug>
#include <QFile>
#include <QTextStream>

#define logDebug qDebug()

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
// maybe_todo: do something else about the quotes - or suggest
// user to upgrade their qt-version...
#define logInfo qWarning()  // no info yet...
#define logWarning qWarning()
#define logCritical qCritical()
#elif QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
#define logInfo qWarning().noquote()  // no info yet...
#else
#define logInfo qInfo().noquote()
#define logWarning qWarning().noquote()
#define logCritical qCritical().noquote()
#endif


namespace logger {


class LogRotate{
public:
    LogRotate(QString fullpath=QString());
    void setup();
    void cleanup();
    void setFullpath(const QString& p);

    QTextStream& stream();


    const QFile& file() const;



private:
    void openLogfileOrThrow();

    QString m_fullpath;
    QFile m_file;
    QTextStream m_stream;

};


const char* msgTypeToStr(QtMsgType msgType);
int msgTypeToOrdinal(QtMsgType msgType);
QtMsgType strToMsgType(const char* str);


void setup(const QString &preamble);
void enableLogToFile(const QString &filename);
void disableLogToFile();
void setVerbosityLevel(QtMsgType lvl);
void setVerbosityLevel(const char* str);
QtMsgType getVerbosityLevel();


LogRotate& getLogRotate();

const QString &logDir();


}




