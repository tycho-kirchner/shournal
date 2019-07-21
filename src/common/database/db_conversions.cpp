
#include <QDateTime>
#include "db_conversions.h"
#include "util.h"

QVariant db_conversions::fromMtime(time_t mtime)
{
    return QVariant(QDateTime::fromTime_t(static_cast<uint>(mtime)));
}

/// sqlite cannot store uint64 as int - store as blob instead.
QVariant db_conversions::fromHashValue(const HashValue &val)
{
    QByteArray hashBytes;
    if(! val.isNull()){
        hashBytes = qBytesFromVar(val.value());
    }
    return QVariant(hashBytes);
}

HashValue db_conversions::toHashValue(const QVariant &var)
{
    if(var.isNull()){
        return {};
    }
    return varFromQBytes(var.toByteArray(), HashValue::value_type(0));
}
