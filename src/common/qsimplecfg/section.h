
#pragma once

#include <unordered_set>
#include <unordered_map>
#include <QDebug>
#include <QString>
#include <QHash>
#include <QVariant>

#include "ordered_map.h"
#include "exccfg.h"
#include "util.h"
#include "generic_container.h"



namespace qsimplecfg {

/// A config section consisting of optional initial comments
/// and key-value pairs. Example
/// # Some comment
/// key1 = val1
/// key2 = val2
///
/// If insertDefaultToComments is true, add a comment of the form
/// key = defaultValue when getValue is called, as hint for the user.
/// Note that comments are ignored when parsing the config file, they are
/// set by the application (and written to the file on store).
class Section
{
public:

    qint64 getFileSize(const QString & key, const qint64& defaultValue={},
                       bool insertDefaultIfNotExist=false );

    template <typename T>
    T getValue(const QString & key, const T& defaultValue=T(),
               bool insertDefaultIfNotExist=false);

    template<class ContainerT>
    ContainerT getValues(const QString & key, const ContainerT & defaultValue=ContainerT(),
                         bool insertDefaultIfNotExist=false,
                         const QString & separator=",",
                         QString::SplitBehavior splitbehaviour=QString::SkipEmptyParts );


    void setComments(const QString &comments);

    const QString & comments() const;

    void setInsertDefaultToComments(bool insertDefaultToComments);

    const std::unordered_set<QString>& notReadKeys() const;

    const QString &sectionName() const;

    bool renameParsedKey(const QString& oldName, const QString& newName);

public:
    ~Section() = default;
    Q_DISABLE_COPY(Section)
    DISABLE_MOVE(Section)

private:
    struct ValueWithMeta {
        QString rawStr; // isNull() == true, if not parsed.
        QString separator;
        QVariantList defaultValues;
        bool insertDefault {false};
        bool insertDefaultToComments {true};
    };

private:
    friend class Cfg;

    typedef tsl::ordered_map<QString, ValueWithMeta> KeyMetaValHash;
    // methods to be called from class Cfg:
    Section(const QString& sectionName);
    void setSectionName(const QString &sectionName);
    void insert(const QString & key, const QString& value);
    const KeyMetaValHash& keyValHash();
    QString generateComments();

private:

    void removeFromNotReadKeysIfExist(const QString& key);
    template <typename T>
    T convertValueOrThrow(const QString& valueStr, const QString& keyname);
    ValueWithMeta& generateValueWithMeta(const QString& key, const QString &separator,
                                         const QVariantList &defaultValues,
                                         bool insertDefaultIfNotExist);

    QString m_comments;
    std::unordered_map<QString, QString> m_parsedKeyValHash; // parsed from file
    KeyMetaValHash m_keyValHash; // accessed by user via getValue(s)
    bool m_insertDefaultToComments { true };
    std::unordered_set<QString> m_NotReadKeys;
    QString m_sectionName;
};



/// Find the 'value' correspondig to 'key' and try to convert it
/// to the target type (throws on error). If the value is not
/// found or is empty, the default one is returned, which is also inserted
/// into the section, if param insertDefaultIfNotExist is true.
/// The default value *must* be convertible to a string using qVariantTo<>.
/// If possible, the default is stored as hint in the comments.
/// @throws ExcCfg
template <typename T>
T Section::getValue(const QString & key, const T& defaultValue,
           bool insertDefaultIfNotExist){
#ifndef NDEBUG
    QString assertTmpResult;
    assert(qVariantTo(defaultValue, &assertTmpResult) );
#endif
    auto & valWithMeta = generateValueWithMeta(key, QString(), { QVariant::fromValue(defaultValue) },
                                                      insertDefaultIfNotExist);
    if(valWithMeta.rawStr.isNull()){
        // not parsed, return default;
        return defaultValue;
    }
    return convertValueOrThrow<T>(valWithMeta.rawStr, key);
}


/// Similar to getValue, but support for multiple values stored within the
/// same key. The single-value container, whose elements are of ValT is then
/// filled. The value is *always* trimmed.
/// @throws ExcCfg
template<class ContainerT>
ContainerT Section::getValues(const QString &key, const ContainerT &defaultValue,
                              bool insertDefaultIfNotExist,
                              const QString &separator,
                              QString::SplitBehavior splitbehaviour)
{
    QVariantList defaultVariantValues;
    for(const auto & val : defaultValue){
#ifndef NDEBUG
        QString assertTmpResult;
        assert(qVariantTo(val, &assertTmpResult) );
#endif
        defaultVariantValues.push_back(QVariant::fromValue(val));
    }
    auto & valWithMeta = generateValueWithMeta(key, separator, defaultVariantValues,
                                                      insertDefaultIfNotExist);
    if(valWithMeta.rawStr.isNull()){
        // not parsed, return default;
        return defaultValue;
    }
    QStringList list = valWithMeta.rawStr.split(separator, splitbehaviour);
    ContainerT container;
    for(const QString & el : list){
        auto parsedVal = convertValueOrThrow<typename ContainerT::value_type>(el, key);
        addToContainer(container, parsedVal);
    }
    return container;
}


////////////////////////////////// private //////////////////////////////////

template<typename T>
T Section::convertValueOrThrow(const QString &valueStr, const QString &keyname)
{
    try {
        return qVariantTo_throw<T>(valueStr, false);
    } catch (const ExcQVariantConvert& ex) {
        throw qsimplecfg::ExcCfg( qtr("%1 (key %2) in section %3")
                                  .arg(ex.descrip(), keyname, m_sectionName));
    }
}

} // namespace qsimplecfg
