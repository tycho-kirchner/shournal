#pragma once

#include <string>
#include <exception>

namespace os {

class ExcOsCommon : public std::exception
{
public:
    ExcOsCommon(std::string  text);

    const char *what () const noexcept override;
protected:
    ExcOsCommon();

    std::string m_descrip;
};


/// Exception with custom preamble which automatically
/// determines errno and builds an error description string
/// on what().
class ExcOs : public ExcOsCommon
{
public:
    ExcOs(const std::string & preamble=std::string());
    ExcOs(const std::string & preamble, int errorNumber);
    int errorNumber() const;

protected:
    int m_errorNumber;
};

class ExcTooFewBytesWritten : public ExcOs {
public:
    using ExcOs::ExcOs;
};


class ExcReadLink : public ExcOs {
public:
    using ExcOs::ExcOs;
};

class ExcProcessExitNotNormal : public ExcOsCommon {
public:
    enum TypeOfTerm { SIG, COREDUMP, NOT_IMPLEMENTED };

    ExcProcessExitNotNormal(int status, TypeOfTerm typeOfTerm);

    int status() const;

    TypeOfTerm typeOfTermination() const;

protected:
    int m_status;
    TypeOfTerm m_typeOfTermination;
};



} // namespace os
