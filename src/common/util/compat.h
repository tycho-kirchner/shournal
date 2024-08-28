#pragma once

#include <QtGlobal>
#include <QString>
#include <QResource>
#include <QTextStream>
#include <QDateTime>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt
{
    using SplitBehavior = QString::SplitBehavior;
    const SplitBehavior SkipEmptyParts = SplitBehavior::SkipEmptyParts;
    const auto endl = ::endl;

    inline QDateTime datetimeFromDate(const QDate& date){
        return QDateTime(date);
    }
}
#else
namespace Qt
{
    inline QDateTime datetimeFromDate(const QDate& date){
        return date.startOfDay();
    }
}
#endif


#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
namespace Qt
{
    inline bool resourceIsCompressed(QResource &r){
        return r.isCompressed();
    }
}
#else
namespace Qt
{
    inline bool resourceIsCompressed(QResource &r){
        return r.compressionAlgorithm() != QResource::NoCompression;
    }
}
#endif

