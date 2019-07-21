
#include <cassert>

#include "qoutstream.h"


QOut::QOut() :
    m_textStream(stdout)
{

}

QOut::~QOut()
{
    m_textStream.flush();
}


QErr::QErr() :
    m_textStream(stderr)
{

}

QErr::~QErr()
{
    m_textStream.flush();
}



std::function<QString()> QIErr::s_preambleCallback = []() { return ""; };


QIErr::QIErr() :
    m_ts(stderr)
{
    if(s_preambleCallback){
        m_ts << s_preambleCallback();
    }
}

QIErr::~QIErr()
{
    m_ts << endl;
}

void QIErr::setPreambleCallback(const std::function<QString ()> &f){
    s_preambleCallback = f;
}








