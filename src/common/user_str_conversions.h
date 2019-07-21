#pragma once

#include <QString>
#include <QDateTime>

#include "exccommon.h"

class ExcUserStrConversion : public QExcCommon
{
public:
     ExcUserStrConversion(const QString &text);
};


/// Parse datatypes from human input, display them human readable
class UserStrConversions
{
public:
    static const QString& relativeDateTimeUnitDescriptions();

    UserStrConversions();

    qint64 bytesFromHuman(QString str);
    QString bytesToHuman(const qint64 bytes);

    QDateTime relativeDateTimeFromHuman(const QString& str, bool subtractIt);
};

