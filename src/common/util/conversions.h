#pragma once

#include <QString>
#include <QDateTime>

#include "exccommon.h"

class ExcConversion : public QExcCommon
{
public:
     ExcConversion(const QString &text);
};


/// Parse datatypes from human input, display them human readable
class Conversions
{
public:
    static const QString& relativeDateTimeUnitDescriptions();

    qint64 bytesFromHuman(QString str);
    QString bytesToHuman(qint64 bytes);

    QDateTime relativeDateTimeFromHuman(const QString& str, bool subtractIt);

    static const QString& dateIsoFormatWithMilliseconds();
};

