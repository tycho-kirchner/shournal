
#include <cassert>

#include <utility>

#include "qoptarg.h"
#include "compat.h"
#include "exccommon.h"
#include "excoptargparse.h"
#include "util.h"
#include "conversions.h"


/// @param shortName short name, one minus is added to the front (-e)
/// @param name long name, two minus signs are added to the front (--exec)
/// @param description
/// @param hasValue --verbose might be a flag, --size 2 has the value 2.
///
QOptArg::QOptArg(const QString &shortName,
                 const QString &name,
                 QString description,
                 bool hasValue) :
    m_name("--" + name),
    m_description(std::move(description)),
    m_hasValue(hasValue),
    m_argIdx(-1),
    m_internalOnly(false),
    m_isFinalizeFlag(false),
    m_isByteSizeArg(false),
    m_isRelativeDateTime(false),
    m_relativeDateTimeSubtract(false)
{
    if(name.isEmpty()){
        throw QExcIllegalArgument("argname must not be empty");
    }
    if(name.startsWith('-')){
        throw QExcProgramming("please pass names without leading minus");
    }

    if(! shortName.isEmpty()){
        if(shortName.startsWith('-')){
            throw QExcProgramming("please pass short names without leading minus");
        }
        m_shortName = '-' + shortName;
    }

}


/// @param optTrigger
/// @param defaultTriggerStr Trigger string which shall be used,
///                          in case no trigger is entered (by the user)
///
QOptArg::QOptArg(const QString &shortName,
                 const QString &name,
                 const QString &description,
                 const QOptArgTrigger &optTrigger,
                 const QString& defaultTriggerStr) :
    QOptArg(shortName, name, description, true)
{
    m_optTrigger = optTrigger;
    m_defaultTriggerStr = defaultTriggerStr;
}


const QString &QOptArg::shortName() const
{
    return m_shortName;
}

const QString& QOptArg::name() const
{
    return m_name;
}

QString QOptArg::description() const
{
    return m_description  ;
}


bool QOptArg::hasValue() const
{
    return m_hasValue;
}


bool QOptArg::wasParsed() const
{
    return m_argIdx != -1;
}


const QOptArgTrigger &QOptArg::optTrigger() const
{
    return m_optTrigger;
}

/// Meant to be overidden by subclasses.
/// Called right before a potential trigger word is further
/// processed.
QString QOptArg::preprocessTrigger(const char *str) const
{
    return str;
}

const QString &QOptArg::parsedTrigger() const
{
    return m_parsedTrigger;
}

void QOptArg::setParsedTrigger(const QString &parsedTrigger)
{
    m_parsedTrigger = parsedTrigger;
}

/// See also: setAllowedOptions()
/// @param maxCount: throw, in case more options than maxCount were parsed.
QStringList QOptArg::getOptions(int maxCount) const
{
    if(m_allowedOptions.empty()){
        throw QExcProgramming(QString("%1 called without previous setAllowedOptions")
                               .arg(__func__));
    }
    if(! m_hasValue){
        throwgetValueCalledOnFlag(__func__);
    }

    QStringList valList;
    for(int i=0; i < m_vals.len; i++){
        QStringList newVals = QString(m_vals.argv[i])
                .split(m_allowedOptionsDelimeter, Qt::SkipEmptyParts);
        for(const QString& str : newVals){
            if(m_allowedOptions.find(str) == m_allowedOptions.end()){
                throw ExcOptArgParse(qtr("'%1' is not a supported option for '%2'. ")
                                     .arg(str, m_name));
            }
        }
        valList += newVals;
        if(valList.size() > maxCount){
            throw ExcOptArgParse(qtr("Only %1 option(s) allowed for argument %2")
                                 .arg(maxCount).arg(m_name));
        }
    }
    return valList;
}

/// Note: this argument must have been marked as 'bytesize' beforehand
QVariantList QOptArg::getVariantByteSizes(const QVariantList &defaultValues)
{
    assert(m_isByteSizeArg);
    auto sizeStrs = getVariantValues<QString>(defaultValues);
    QVariantList sizes;
    Conversions userStrConv;
    for(const auto& s : sizeStrs){
        try {
            sizes.push_back(userStrConv.bytesFromHuman(s.toString()));
        } catch (const ExcConversion& e) {
            throw ExcOptArgParse(e.descrip() + " (arg " + m_name + ')' );
        }
    }
    return sizes;
}

QVariantList QOptArg::getVariantRelativeDateTimes(const QVariantList &defaultValues)
{
    assert(m_isRelativeDateTime);
    auto dateTimeStrs = getVariantValues<QString>(defaultValues);
    QVariantList dateTimes;
    Conversions userStrConv;
    for(const auto& s : dateTimeStrs){
        try {
            dateTimes.push_back(userStrConv.relativeDateTimeFromHuman(s.toString(),
                                                                      m_relativeDateTimeSubtract));
        } catch (const ExcConversion& e) {
            throw ExcOptArgParse(e.descrip() + " (arg " + m_name + ')' );
        }
    }
    return dateTimes;
}



/// See also: getOptions(), where the check is performed lazily.
void QOptArg::setAllowedOptions(const std::unordered_set<QString> &options,
                                const QString &delimeter)
{
    m_allowedOptions = options;
    if(delimeter.isEmpty()){
        throw QExcIllegalArgument(QString("%1: empty delimeter passed.").arg(__func__));
    }
    m_allowedOptionsDelimeter = delimeter;
}

const QOptArg::RawValues_t &QOptArg::vals() const
{
    return m_vals;
}

void QOptArg::setVals(const RawValues_t &vals)
{
    m_vals = vals;
}

int QOptArg::argIdx() const
{
    return m_argIdx;
}

void QOptArg::setArgIdx(int argIdx)
{
    m_argIdx = argIdx;
}

const QString &QOptArg::defaultTriggerStr() const
{
    return m_defaultTriggerStr;
}

/// see setter
bool QOptArg::internalOnly() const
{
    return m_internalOnly;
}

/// An internal argument is not displayed in the help
void QOptArg::setInternalOnly(bool internalOnly)
{
    m_internalOnly = internalOnly;
}

/// If *this* argument is parsed, param arg must be parsed
/// as well.
void QOptArg::addRequiredArg(const QOptArg *arg)
{
    assert(arg->name() != this->name());
    m_requiredArs.append(arg);
}

const QVector<const QOptArg *>& QOptArg::requiredArs() const
{
    return m_requiredArs;
}

void QOptArg::throwgetValueCalledOnFlag(const char *functionname) const
{
    throw QExcProgramming(QString("%1() was called although argument %2 "
                                  "was marked as flag (no value)").arg(functionname, m_name));
}

/// @param subtractIt: if true, the parsed date is subtracted from current one,
/// else it is added.
void QOptArg::setIsRelativeDateTime(bool isRelativeDateTime, bool subtractIt)
{
    m_isRelativeDateTime = isRelativeDateTime;
    m_relativeDateTimeSubtract = subtractIt;
    m_description += qtr(" Supported units include %1")
                          .arg(Conversions::relativeDateTimeUnitDescriptions());
}


/// adds a description, that this argument also accepts bytesizes after given
/// numbers lik KiB, MiB, etc.
void QOptArg::setIsByteSizeArg(bool isByteSizeArg)
{
    m_isByteSizeArg = isByteSizeArg;
    m_description += qtr(" You may provide a unit such as KiB, MiB, etc..");
}

bool QOptArg::isFinalizeFlag() const
{
    return m_isFinalizeFlag;
}

/// Currently only supported for flags (arguments without values).
/// If true, the parser will stop processing args, if argument is passed.
/// This can be used to delegate parsing control to a 'sub-parser'.
/// Therefor, if finalize is true, an exception will be thrown, if no furhter
/// arguments are available after the respective flag.
/// Default is false.
void QOptArg::setFinalizeFlag(bool f)
{
    if(m_hasValue){
        throw QExcProgramming("Finalize flag currently only supported for "
                              "flags (arguments without value)");
    }
    m_isFinalizeFlag = f;
}

const QString &QOptArg::allowedOptionsDelimeter() const
{
    return m_allowedOptionsDelimeter;
}

const std::unordered_set<QString>& QOptArg::allowedOptions() const
{
    return m_allowedOptions;
}









