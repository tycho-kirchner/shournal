
#include <utility>
#include <QDebug>

#include "exccommon.h"
#include "util.h"
#include "translation.h"



ExcCommon::ExcCommon(std::string text) :
    m_descrip(std::move(text))
{}

const char *ExcCommon::what() const noexcept
{
    return m_descrip.c_str();
}

std::string &ExcCommon::descrip()
{
    return m_descrip;
}




QExcCommon::QExcCommon(QString text, bool collectStacktrace) :
    m_descrip(std::move(text))
{
    if(collectStacktrace){
       appendStacktraceToDescrip();
    }

}

const char *QExcCommon::what() const noexcept
{
    m_local8Bit = m_descrip.toLocal8Bit();
    return m_local8Bit.constData();
}

QString QExcCommon::descrip() const
{
    return m_descrip;
}

void QExcCommon::setDescrip(const QString &descrip)
{
    m_descrip = descrip;
    m_local8Bit = descrip.toLocal8Bit();
}

void QExcCommon::appendStacktraceToDescrip()
{
    const auto st = generate_trace_string();
    m_descrip += "\n" + QString::fromStdString(st);
}



QExcIllegalArgument::QExcIllegalArgument(const QString &text) :
    QExcCommon (text)
{

}

QExcProgramming::QExcProgramming(const QString &text) :
    QExcCommon (text)
{

}

QExcIo::QExcIo(QString text, bool collectStacktrace) :
    QExcCommon("", false)
{
    m_errorNumber = errno;
    if(errno != 0){
        text += " (" + QString::number(errno) +
                "): " + translation::strerror_l(errno);
    }

    this->setDescrip(text);
    if(collectStacktrace){
        appendStacktraceToDescrip();
    }
}

int QExcIo::errorNumber() const
{
    return m_errorNumber;
}
