#pragma once

#include <QString>
#include <QVector>
#include <unordered_set>

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

    QString shortName() const;

    QString name() const;

    virtual QString description() const;

    bool hasValue() const;

    // after parse:
    bool wasParsed() const;

    const QOptArgTrigger& optTrigger() const;
    virtual QString preprocessTrigger(const char* str) const;

    const QString& parsedTrigger() const;
    virtual void setParsedTrigger(const QString &parsedTrigger);


    template <typename T>
    T getValue(const T& defaultValue=T());

    template <typename ContainerT>
    ContainerT getValues(const ContainerT& defaultValues={});

    template <typename T>
    QVariantList getVariantValues(const QVariantList& defaultValues={});

    QStringList getOptions(int maxCount=std::numeric_limits<int>::max());
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
    void throwgetValueCalledOnFlag(const char* functionname);

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
T QOptArg::getValue(const T& defaultValue){
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


