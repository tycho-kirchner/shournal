#pragma once

#include <QProcess>

#include "os.h"

/// QProcess which throws on several functions.
/// Also sets some properties to different default
/// (see constructor) and provides the ability
/// to launch interactive processes by forwarding stdin.
/// Furthermore a fd may be inherited, to also wait
/// for possible subprocesses of the launched process
/// (which only works, if that process also inherits them..)
/// In waitForFinished it is then waited until the fd is closed
/// by all subprocesses (until all processes finished).
class QProcThrow : public QProcess
{
    Q_OBJECT
public:
    QProcThrow(bool forwardStdin=false,
               bool waitForInheritedFd=false);

    void start(const QString &program, const QStringList &arguments, OpenMode mode = ReadWrite);
#if !defined(QT_NO_PROCESS_COMBINED_ARGUMENT_START)
    void start(const QString &command, OpenMode mode = ReadWrite);
#endif
    void start(OpenMode mode = ReadWrite);



    void waitForStarted(int msecs = 30000);
    void waitForFinished(int msecs = 30000);
    void wait(int msecs = 30000);

protected:
    virtual void setupChildProcess();


private:
    void setupInheritableFdsIfOn();

    os::Pipes_t m_inheritFds;

    static int s_stdinClone;
    bool m_forwardStdin;
    bool m_waitForInhertitedFd;

};

