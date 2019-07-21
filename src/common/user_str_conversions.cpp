#include <cassert>
#include <unordered_set>

#include <QStringList>
#include <QRegularExpression>

#include "user_str_conversions.h"
#include "util.h"

namespace  {

QHash<QString, char> validTimeUnitHash(){
    static const QHash<QString, char> units {
        {qtr("y"), 'y'}, // Year
        {qtr("m"), 'm'},   // month
        {qtr("d"), 'd'},   // day
        {qtr("h"), 'h'},   // hour
        {qtr("min"), 'M'},   // minute
        {qtr("s"), 's'},   // second
    };
    return units;
}


} // namespace


ExcUserStrConversion::ExcUserStrConversion(const QString  & text) :
    QExcCommon(text, false)
{}



UserStrConversions::UserStrConversions()
= default;



/// @returns by comma separated list of valid relative time units with description
/// (y: year, m: month, ...).
const QString &UserStrConversions::relativeDateTimeUnitDescriptions()
{
    static const auto s = qtr("y: year, m: month, d: day, h: hour, min: minute, s: second");
    return s;
}


/// Transform user supplied byte-sizes ("3KiB", "2 MiB ", etc.) to int.
/// @throws ExcUserStrConversion
qint64 UserStrConversions::bytesFromHuman(QString str)
{
    str = str.simplified();
    str.replace( " ", "" );

    const QString errPreamble(qtr("Failed to convert bytesize '%1' - ").arg(str));

    if(str.isEmpty()){
        throw ExcUserStrConversion(errPreamble + qtr("it is empty."));
    }
    if(str[str.size() - 1].isDigit()){
        // assuming bytes size
        qint64 bytes;
        if(! qVariantTo(str, &bytes)){
            throw ExcUserStrConversion(errPreamble + qtr("it appears to be not an integer "
                                                         "although no unit was given."));
        }
        return bytes;
    }

    int unitIdx = str.size() - 2;
    for(; unitIdx >= 0; unitIdx--){
        if(str[unitIdx].isDigit()){
            unitIdx++;
            break;
        }
    }
    if(unitIdx == -1){
        throw ExcUserStrConversion(errPreamble + qtr("no digit was given"));
    }

    const QString unit = str.mid(unitIdx);
    const QString val = str.left(unitIdx);

    double bytesFloat;
    if(! qVariantTo(val, &bytesFloat)){
        throw ExcUserStrConversion(errPreamble + qtr("conversion from string '%1' to float failed").arg(val));
    }
    if(bytesFloat < 0){
        bytesFloat += -1;
    }

    static const std::unordered_set<QString> validUnitSet {
        "k", "kb", "kib", "m", "mb", "mib", "g", "gb", "gib", "t", "tb", "tib"
    };
    if(validUnitSet.find(unit.toLower()) == validUnitSet.end()){
        const QString validUnits(qtr("valid units include 'no unit', "
                                     "K (Kib), M (MiB), G (GiB) and T (TiB) "
                                     "but '%1' was given").arg(unit));
        throw ExcUserStrConversion(errPreamble + validUnits);
    }

    switch (unit[0].toLower().toLatin1()) {
    case 'k': bytesFloat *= 1024.0; break;
    case 'm': bytesFloat *= 1024.0*1024; break;
    case 'g': bytesFloat *= 1024.0*1024*1024; break;
    case 't': bytesFloat *= 1024.0*1024*1024*1024; break;
    default: assert(false);
    }
    return static_cast<qint64>(bytesFloat);
}


/// size to human readbale string (Kib, Mib, etc....)
QString UserStrConversions::bytesToHuman(const qint64 bytes)
{
    float s = bytes;

    static const QStringList list({"KiB", "MiB", "GiB", "TiB"});
    QStringListIterator i(list);
    QString unit("bytes");
    if(s <= 1024.0f){
        return QString().setNum(s,'f',0)+" "+unit;
    }

    do {
        unit = i.next();
        s /= 1024.0f;
    } while(s >= 1024.0f && i.hasNext());

    return QString().setNum(s,'f',2)+" "+unit;
}

/// @param subtractIt: if true, the parsed date is subtracted from current one,
/// else it is added.
/// @throws ExcUserStrConversion
QDateTime UserStrConversions::relativeDateTimeFromHuman(const QString &str, bool subtractIt)
{
    static const QRegularExpression re(R"((\d+)(.+))");
    QRegularExpressionMatch match = re.match(str);
    const QString errPreamble(qtr("Failed to convert relative date(time) '%1' - ").arg(str));
    if (! match.hasMatch()) {
        throw ExcUserStrConversion(errPreamble + qtr("It must be a digit followed by a timespec."));
    }

    // must always succeed, otherwise regex would be broken
    int number = match.captured(1).toInt();
    const QString parsedTimeSpec = match.captured(2).trimmed();
    const auto & validUnits = validTimeUnitHash();

    auto matchedUnitIt = validUnits.find(parsedTimeSpec);

    if(matchedUnitIt == validUnits.end()){
        // don't use auto here: older version of qt do not support QList<QString>::join...
        QStringList units = validUnits.keys();
        throw ExcUserStrConversion(errPreamble + qtr("%1 is not a valid timespec. Those are %2")
                                                    .arg(units.join(",")));
    }

    if(subtractIt){
        // go back in time:
        number = -number;
    }

    auto now = QDateTime::currentDateTime();
    switch (matchedUnitIt.value()) {
    case 'y': return now.addYears(number);
    case 'm': return now.addMonths(number);
    case 'd': return now.addDays(number);
    case 'h': return now.addSecs(number*3600);
    case 'M': return now.addSecs(number*60);
    case 's': return now.addSecs(number);
    default:
        assert(false);
    }
    return {};
}


