#pragma once

#include <QString>
#include <QVector>
#include <unordered_set>

#include "compat.h"
#include "util.h"
#include "qoptargtrigger.h"
#include "excoptargparse.h"

class QOptArg {
public:
    struct RawValues_t{
        RawValues_t() : argv(nullptr), len(0) {}
        char** argv;
        int len;
    };

    QOptArg(const QString& shortName, const QString & name,
            QString  description,
            bool hasValue=true );

    QOptArg(const QString& shortName, const QString & name,
            const QString& description,
            const QOptArgTrigger & optTrigger, const QString& defaultTriggerStr);

    virtual ~QOptArg() = default;

    const QString& shortName() const;

    const QString& name() const;

    virtual QString description() const;

    bool hasValue() const;

    // after parse:
    bool wasParsed() const;

    const QOptArgTrigger& optTrigger() const;
    virtual QString preprocessTrigger(const char* str) const;

    const QString& parsedTrigger() const;
    virtual void setParsedTrigger(const QString &parsedTrigger);


    template <typename T>
    T getValue(const T& defaultValue=T()) const;

    template <typename ContainerT>
    ContainerT getValues(const ContainerT& defaultValues={});

    template <typename ContainerT>
    ContainerT getValuesByDelim(const QString& delim=",", const ContainerT& defaultValues={},
                                const int minValueSize=1,
                                const int maxValueSize=std::numeric_limits<int>::max());


    template <typename T>
    QVariantList getVariantValues(const QVariantList& defaultValues={});

    QStringList getOptions(int maxCount=std::numeric_limits<int>::max()) const;
    QVariantList getVariantByteSizes(const QVariantList& defaultValues={});
    QVariantList getVariantRelativeDateTimes(const QVariantList& defaultValues={});

    void setAllowedOptions(const std::unordered_set<QString>& options,
                           const QString&delimeter=",");
    const std::unordered_set<QString>& allowedOptions() const;
    const QString& allowedOptionsDelimeter() const;


    const RawValues_t& vals() const;
    void setVals(const RawValues_t &vals);

    int argIdx() const;
    void setArgIdx(int argIdx);

    const QString& defaultTriggerStr() const;

    bool internalOnly() const;
    void setInternalOnly(bool internalOnly);

    void addRequiredArg(const QOptArg* arg);

    const QVector<const QOptArg *>& requiredArs() const;

    bool isFinalizeFlag() const;
    void setFinalizeFlag(bool f);

    void setIsByteSizeArg(bool isByteSizeArg);

    void setIsRelativeDateTime(bool isRelativeDateTime, bool subtractIt);

protected:
    [[noreturn]]
    void throwgetValueCalledOnFlag(const char* functionname) const;

    QString m_shortName;
    QString m_name;
    QString m_description;
    bool m_hasValue;
    QOptArgTrigger m_optTrigger;
    QString m_defaultTriggerStr;
    int m_argIdx;
    bool m_internalOnly;

    RawValues_t m_vals;
    QVector<const QOptArg*> m_requiredArs;

    // after parse:
    QString m_parsedTrigger;
    std::unordered_set<QString> m_allowedOptions;
    QString m_allowedOptionsDelimeter;
    bool m_isFinalizeFlag;
    bool m_isByteSizeArg;
    bool m_isRelativeDateTime;
    bool m_relativeDateTimeSubtract;
};



/// Get the first value and try to convert it
/// to the target type (throws on error). If the value
/// is empty (not parsed), the default one is returned.
/// @throws ExcCfg
template <typename T>
T QOptArg::getValue(const T& defaultValue) const{
    if(! m_hasValue){
        throwgetValueCalledOnFlag(__func__);
    }

    if(m_vals.len == 0){
        return defaultValue;
    }
    T t;
    try {
        qVariantTo_throw(m_vals.argv[0], &t, false);
    } catch (const ExcQVariantConvert& ex) {
        throw ExcOptArgParse(ex.descrip() + " (arg " + m_name + ')' );
    }
    return t;
}


/// Try to convert all values
/// to the target type (throws on error). If the values
/// are empty (not parsed), the default ones are returned.
/// @throws ExcCfg
template <typename ContainerT>
ContainerT QOptArg::getValues(const ContainerT& defaultValues){
    if(! m_hasValue){
        throwgetValueCalledOnFlag(__func__);
    }
    if(m_vals.len == 0){
        return defaultValues;
    }
    ContainerT container;
    for(int i=0; i < m_vals.len; i++){
        typename ContainerT::value_type t;
        try {
            qVariantTo_throw(m_vals.argv[i], &t, false);
        } catch (const ExcQVariantConvert& ex) {
            throw ExcOptArgParse(ex.descrip() + " (arg " + m_name + ')' );
        }
        container.push_back(t);
    }
    return container;

}

/// for a *single* argument string, whose values are separated by a delimter (e.g. comma)
/// argFoo 1,2,3
template <typename ContainerT>
ContainerT QOptArg::getValuesByDelim(const QString& delim, const ContainerT& defaultValues,
                                     const int minValueSize, const int maxValueSize){
    if(! m_hasValue){
        throwgetValueCalledOnFlag(__func__);
    }
    if(m_vals.len == 0){
        return defaultValues;
    }
    ContainerT container;
    const auto splittedVals = QString(m_vals.argv[0]).split(delim, Qt::SkipEmptyParts);
    if(splittedVals.size() < minValueSize || splittedVals.size() > maxValueSize){
         throw ExcOptArgParse(qtr("argument %1 requires at least %2 and at most %3 "
                                  "parameters, separated by '%4' but %5 were given.")
                                    .arg(m_name).arg(minValueSize).arg(maxValueSize)
                                    .arg(delim).arg(splittedVals.size()));
    }
    for(const QString & val : splittedVals){
        typename ContainerT::value_type t;
        try {
            qVariantTo_throw(val, &t, false);
        } catch (const ExcQVariantConvert& ex) {
            throw ExcOptArgParse(ex.descrip() + " (arg " + m_name + ')' );
        }
        container.push_back(t);
    }
    return container;
}


/// Same as getValues(), but returns a QVariantList.
/// The template parameter is there to convert the values into
/// the target type right here.
template<typename T>
QVariantList QOptArg::getVariantValues(const QVariantList& defaultValues)
{
    if(! m_hasValue){
        throwgetValueCalledOnFlag(__func__);
    }
    if(m_vals.len == 0){
        return defaultValues;
    }
    QVariantList l;
    for(int i=0; i < m_vals.len; i++){
        T t;
        try {
            qVariantTo_throw(m_vals.argv[i], &t, false);
        } catch (const ExcQVariantConvert& ex) {
            throw ExcOptArgParse(ex.descrip() + " (arg " + m_name + ')' );
        }
        l.push_back(t);
    }
    return l;
}


