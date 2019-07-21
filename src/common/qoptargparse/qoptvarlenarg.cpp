#include "qoptvarlenarg.h"




QOptVarLenArg::QOptVarLenArg(const QString &shortName,
                             const QString &name,
                             const QString &description) :
    QOptArg(shortName, name, description, true)
{}
