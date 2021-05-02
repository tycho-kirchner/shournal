#pragma once

#include <QTextStream>
#include <functional>


/// Print QString's and other compatible types
/// to stdout (flush on destructor).
class QOut
{
public:
    QOut();
    ~QOut();

    template<class T>
    QOut& operator<<(const T& t) {
        m_textStream << t;
        return *this;
    }
private:
    QTextStream m_textStream;
};


/// Print QString's and other compatible types
/// to stderr (flush on destructor).
class QErr
{
public:
    QErr();
    ~QErr();
    template<class T>
    QErr& operator<<(const T& t) {
        m_textStream << t;
        return *this;
    }
private:
    QTextStream m_textStream;
};



/// Informative QErr.
/// Wrap stderr and add a custom preamble before every stream start.
/// Print a custom message in the constructor.
/// Auto-separate words by whitespace.
/// In destructor, add 'newline '\n' and flush (endl)
/// Use it like:
/// QICerr::setPreambleCallback([]() { return QCoreApplication::applicationName() + ": "; });
/// QICerr() << "Foo" << "bar";
class QIErr
{
public:
    QIErr();
    ~QIErr();

    template<class T>
    QIErr& operator<<(const T& t) {
        if(m_WrittenTo){
            // auto whitespace
            m_ts << ' ';
        } else {
            m_WrittenTo = true;
        }
        m_ts << t;
        return *this;
    }

    static void setPreambleCallback(const std::function<QString()>& f);
private:
    bool m_WrittenTo {false};
    QTextStream m_ts;
    static std::function<QString()> s_preambleCallback;

};



