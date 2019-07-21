#pragma once

#include <QString>
#include <QHash>

#include "qoptarg.h"
#include "orderedmap.h"

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
    OrderedMap<QString, QOptArg*> m_args;
    OrderedMap<QString, QOptArg*> m_argsShort;
    QOptArg::RawValues_t m_rest;
    QString m_helpIntroduction;

    void printHelp();
};

