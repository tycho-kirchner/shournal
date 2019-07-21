#pragma once

#include "qoptarg.h"

class QOptVarLenArg : public QOptArg
{
public:
    QOptVarLenArg(const QString& shortName, const QString & name,
                  const QString& description);
};

