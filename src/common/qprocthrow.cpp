
#include "qprocthrow.h"
#include "excqprocthrow.h"
#include "os.h"
#include "excos.h"
#include "util.h"

#include <unistd.h>
#include <QDebug>

int QProcThrow::s_stdinClone = -1;

QProcThrow::QProcThrow(bool forwardStdin, bool waitForInheritedFd) :
    m_inheritFds(),
    m_forwardStdin(forwardStdin),
    m_waitForInhertitedFd(waitForInheritedFd)
{
    m_inheritFds[0] = -1;
    this->setProcessChannelMode(QProcess::ProcessChannelMode::ForwardedChannels);
    if (m_forwardStdin && s_stdinClone == -1){
        s_stdinClone = ::dup(fileno(stdin));
    }
}

void QProcThrow::start(const QString &program, const QStringList &arguments, QIODevice::OpenMode mode)
{
    setupInheritableFdsIfOn();
    QProcess::start(program, arguments, mode);
}

void QProcThrow::start(const QString &command, QIODevice::OpenMode mode)
{
    setupInheritableFdsIfOn();
    QProcess::start(command, mode);
}

void QProcThrow::start(QIODevice::OpenMode mode)
{
    setupInheritableFdsIfOn();
    QProcess::start(mode);
}


void QProcThrow::waitForStarted(int msecs)
{
    if(! QProcess::waitForStarted(msecs)){
        throw ExcQProcThrow(this->errorString());
    }
}

void QProcThrow::waitForFinished(int msecs)
{
    if(m_inheritFds[0] != -1){
        os::close(m_inheritFds[1]);
        char c;
        os::read(m_inheritFds[0], &c, 1, true);
        os::close(m_inheritFds[0]);
    }

    if(! QProcess::waitForFinished(msecs)){
        throw ExcQProcThrow(this->errorString());
    }
}

void QProcThrow::wait(int msecs)
{
    this->waitForStarted(msecs);
    this->waitForFinished(msecs);
}

void QProcThrow::setupChildProcess()
{
    if(m_forwardStdin){
        ::dup2(s_stdinClone, fileno(stdin));
    }
}

void QProcThrow::setupInheritableFdsIfOn()
{
    if(m_waitForInhertitedFd){
        m_inheritFds = os::pipe(0);
    } else {
        m_inheritFds[0] = -1;
    }
}
