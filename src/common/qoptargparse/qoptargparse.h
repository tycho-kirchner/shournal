#pragma once

#include <QString>
#include <QHash>

#include "qoptarg.h"
#include "ordered_map.h"

/// Currently no support for having the same argument multiple times
class QOptArgParse
{
public:
    QOptArgParse();
    void addArg(QOptArg* arg );

    void parse(int argc, char *argv[]);

    QOptArg::RawValues_t& rest();

    void setHelpIntroduction(const QString& txt);

private:
    tsl::ordered_map<QString, QOptArg*> m_args;
    tsl::ordered_map<QString, QOptArg*> m_argsShort;
    QOptArg::RawValues_t m_rest;
    QString m_helpIntroduction;

    void printHelp();
};

