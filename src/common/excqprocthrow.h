#pragma once

#include <QProcess>
#include "exccommon.h"

class ExcQProcThrow : public QExcCommon
{
public:
    ExcQProcThrow(const QString & errorStr);
};

