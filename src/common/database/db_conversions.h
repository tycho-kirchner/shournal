#pragma once

#include <QVariant>
#include <time.h>

#include "nullable_value.h"

namespace db_conversions {
    QVariant fromMtime(time_t mtime);
    // Not toMtime, because we work with QDateTime afterwards

    QVariant fromHashValue(const HashValue& val);
    HashValue toHashValue(const QVariant& var);
}


