
#include <utility>
#include <QDebug>

#include "exccommon.h"
#include "util.h"



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
    m_descrip(std::move(text)),
  m_local8Bit(m_descrip.toLocal8Bit())
{
    if(collectStacktrace){
        const auto st = generate_trace_string();
        m_descrip += "\n" + QString::fromStdString(st);
        m_local8Bit += "\n" + QByteArray(st.c_str(), static_cast<int>(st.size()));
    }

}

const char *QExcCommon::what() const noexcept
{
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



QExcIllegalArgument::QExcIllegalArgument(const QString &text) :
    QExcCommon (text)
{

}

QExcProgramming::QExcProgramming(const QString &text) :
    QExcCommon (text)
{

}
