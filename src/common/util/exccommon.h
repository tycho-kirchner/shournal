#pragma once

#include <exception>
#include <string>
#include <QString>

class ExcCommon : public std::exception
{
public:
    explicit ExcCommon(std::string  text);

    const char *what () const noexcept override;
    std::string & descrip();

protected:
    std::string m_descrip;
};



class QExcCommon : public std::exception
{
public:
    explicit QExcCommon(QString  text, bool collectStacktrace=true);

    const char *what () const noexcept override;
    QString descrip() const;
    void setDescrip(const QString &descrip);

protected:
    void appendStacktraceToDescrip();

    QString m_descrip;

private:
    mutable QByteArray m_local8Bit;
};


class QExcIllegalArgument : public QExcCommon
{
public:
    QExcIllegalArgument(const QString & text);
};

/// Thrown in case of a detected bug^^
class QExcProgramming : public QExcCommon
{
public:
    QExcProgramming(const QString & text);
};


class QExcIo : public QExcCommon
{
public:
    explicit QExcIo(QString  text, bool collectStacktrace=true);
    int errorNumber() const;
private:
    int m_errorNumber;
};
