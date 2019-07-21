
#pragma once

#include <QDebug>
#include <QString>
#include <QHash>
#include <QVariant>
#include "unordered_set"

#include "orderedmap.h"
#include "exccfg.h"
#include "util.h"
#include "generic_container.h"



namespace qsimplecfg {

/// A config section consisting of optional initial comments
/// a key-value pairs. Exmaple
/// # Some comment
/// key1 = val1
/// key2 = val2
///
/// If insertDefaultToComments is true, add a comment of the form
/// key = defaultValue when getValue is called, as hint for the user.
/// Note that comments are ignored when parsing the config file, but are
/// set by the application (and written to the file).
class Section
{
public:
    struct ValueWithMeta {
        ValueWithMeta() : multiValue(false){}
        QString rawStr;
        bool multiValue;
        QString separator;
    };


    Section(const QString& sectionName);

    void insert(const QString & key, const QString& value);

    qint64 getFileSize(const QString & key, const qint64& defaultValue={},
                       bool insertDefaultIfNotExist=false );

    template <typename T>
    T getValue(const QString & key, const T& defaultValue=T(),
               bool insertDefaultIfNotExist=false);

    template<class ContainerT>
    ContainerT getValues(const QString & key, const ContainerT & defaultValue=ContainerT(),
                         bool insertDefaultIfNotExist=false,
                         bool simplified=true,
                         const QString & seperator=",",
                         QString::SplitBehavior splitbehaviour=QString::SkipEmptyParts );

    const OrderedMap<QString, ValueWithMeta>& keyValHash();

    void setComments(const QString &comments);

    const QString & comments() const;

    void setInsertDefaultToComments(bool insertDefaultToComments);

    const std::unordered_set<QString>& notReadKeys() const;

    bool sectionWasRead() const;
    void setSectionWasRead(bool sectionWasRead);

    void updateValuesFromOtherSection(const Section& other);

    const QString &sectionName() const;

private:


    void removeFromNotReadKeysIfExist(const QString& key);

    QString m_comments;
    OrderedMap<QString, ValueWithMeta> m_keyValHash;
    bool m_insertDefaultToComments;
    std::unordered_set<QString> m_NotReadKeys;
    bool m_sectionWasRead;
    QString m_sectionName;
};




/// Find the value correspondig to key and try to convert it
/// to the target type (throws on error). If the value is not
/// found or is empty, the default one is returned, which is also inserted
/// into the section, if param insertDefaultIfNotExist is true.
/// If possible, the default is stored as hint in the comments.
/// @throws ExcCfg
template <typename T>
T Section::getValue(const QString & key, const T& defaultValue,
           bool insertDefaultIfNotExist){
    removeFromNotReadKeysIfExist(key);

    QString defaultValueStr;
    try {
        defaultValueStr += qVariantTo_throw<QString>(defaultValue);
        if(m_insertDefaultToComments){
            QString optTripleStart;
            QString optTripleEnd;
            if(defaultValueStr.trimmed().contains("\n")){
                optTripleStart = "'''\n";
                if(defaultValueStr.endsWith('\n')){
                     optTripleEnd = "'''";
                } else {
                     optTripleEnd = "\n'''";
                }
            }
            m_comments += key + " = " + optTripleStart + defaultValueStr + optTripleEnd + "\n";
        }
    } catch (const ExcQVariantConvert& ex) {
        if(insertDefaultIfNotExist){
            throw QExcProgramming(QString("Would be unable to insert default for key %1 - %2")
                                  .arg(key, ex.descrip()));
        } else{
            qDebug() << key << ex.descrip();
        }
    }
    auto valueIter = m_keyValHash.find(key);
    if(valueIter == m_keyValHash.end()){
        if(insertDefaultIfNotExist){
            ValueWithMeta v;
            v.rawStr = defaultValueStr;
            m_keyValHash[key] = v;
        }
        return defaultValue;
    } else {
        T t;
        try {
            qVariantTo_throw(valueIter.value().rawStr, &t, false);
        } catch (const ExcQVariantConvert& ex) {
            throw qsimplecfg::ExcCfg(ex.descrip() +
                                             " (key " + key + ')' );
        }
        return t;
    }
}

/// Similar to getValue, but support for multiple values stored within the
/// same key. The single-value container, whose elements are of ValT is then
/// filled. Per default, the string is QString::simplified and empty elements
/// are ignored. The value is *always* trimmed.
/// If possible, the default is stored as hint in the comments.
/// @throws ExcCfg
template<class ContainerT>
ContainerT Section::getValues(const QString &key, const ContainerT &defaultValue,
                              bool insertDefaultIfNotExist, bool simplified,
                              const QString & seperator,
                              QString::SplitBehavior splitbehaviour)
{
    removeFromNotReadKeysIfExist(key);
    QString defaultValueStr;
    try {
        for(const auto & val : defaultValue){
            defaultValueStr += qVariantTo_throw<QString>(val) + seperator;
        }
        if(m_insertDefaultToComments){
            QString optTripleStart;
            QString optTripleEnd;
            if(seperator == "\n" || defaultValueStr.trimmed().contains("\n")){
                optTripleStart = "'''\n";
                if(defaultValueStr.endsWith('\n')){
                     optTripleEnd = "'''";
                } else {
                     optTripleEnd = "\n'''";
                }

            }
            m_comments +=  key + " = " + optTripleStart + defaultValueStr + optTripleEnd + "\n" ;
        }
    } catch (const ExcQVariantConvert& ex) {
        if(insertDefaultIfNotExist){
            throw QExcProgramming(QString("Would be unable to insert default for key %1 - %2")
                                  .arg(key, ex.descrip()));
        } else {
            qDebug() << key << ex.descrip();
        }
    }
    const auto valueIter = m_keyValHash.find(key);

    if(valueIter == m_keyValHash.end()){
        if(insertDefaultIfNotExist){
            ValueWithMeta v;
            v.rawStr = defaultValueStr;
            v.multiValue = true;
            v.separator = seperator;
            m_keyValHash[key] = v;
        }
        return  defaultValue;
    } else {
        valueIter.value().multiValue = true;
        valueIter.value().separator = seperator;
        QString valueStr;
        if(simplified){
            valueStr = valueIter.value().rawStr.simplified();
        } else {
            valueStr = valueIter.value().rawStr.trimmed();
        }
        QStringList list = valueStr.split(seperator, splitbehaviour);
        ContainerT container;
        for(const QString & el : list){
            typename ContainerT::value_type t;
            try {
                qVariantTo_throw(el, &t, false);
            } catch (const ExcQVariantConvert& ex) {
                throw qsimplecfg::ExcCfg(ex.descrip() + " (key " + key + ')' );
            }
            addToContainer(container, t);
        }
        return container;
    }
}





} // namespace qsimplecfg
