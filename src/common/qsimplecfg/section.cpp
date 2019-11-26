


#include "section.h"
#include "conversions.h"

qsimplecfg::Section::Section(const QString &sectionName) :
    m_sectionName(sectionName)
{}


void qsimplecfg::Section::insert(const QString &key, const QString &value)
{
    m_parsedKeyValHash.insert({key, value});
    m_NotReadKeys.insert(key);
}


/// @see getValue().
qint64 qsimplecfg::Section::getFileSize(const QString &key, const qint64 &defaultValue,
                                        bool insertDefaultIfNotExist)
{
    try {
        Conversions userStrConv;

        return userStrConv.bytesFromHuman( this->getValue<QString>(
                                        key,
                                        userStrConv.bytesToHuman(defaultValue),
                                        insertDefaultIfNotExist));
    } catch (const ExcConversion& ex) {
        throw qsimplecfg::ExcCfg(ex.descrip() + " (key " + key + ')' );
    }
}


const qsimplecfg::Section::KeyMetaValHash &qsimplecfg::Section::keyValHash()
{
    return m_keyValHash;
}


void qsimplecfg::Section::setComments(const QString &comments)
{
    m_comments = comments;
    if(! comments.isEmpty() && comments[comments.size() - 1] != QChar::LineFeed){
        m_comments.push_back(QChar::LineFeed);
    }

}

const QString& qsimplecfg::Section::comments() const
{
    return m_comments;
}

/// Affects subsequent calls to getValue(s): if true, default values will be written
/// to comments on Cfg::store, else not.
void qsimplecfg::Section::setInsertDefaultToComments(bool insertDefaultToComments)
{
    m_insertDefaultToComments = insertDefaultToComments;
}

void qsimplecfg::Section::removeFromNotReadKeysIfExist(const QString &key)
{
    auto it = m_NotReadKeys.find(key);
    if(it != m_NotReadKeys.end()){
        m_NotReadKeys.erase(it);
    }
}

qsimplecfg::Section::ValueWithMeta&
qsimplecfg::Section::generateValueWithMeta(const QString &key, const QString& separator,
                                           const QVariantList& defaultValues,
                                           bool insertDefaultIfNotExist)
{
    removeFromNotReadKeysIfExist(key);
    ValueWithMeta & valWithMeta = m_keyValHash[key];
    valWithMeta.insertDefaultToComments = m_insertDefaultToComments;
    valWithMeta.separator = separator;
    valWithMeta.defaultValues = defaultValues;

    auto parsedValIt = m_parsedKeyValHash.find(key);
    if(parsedValIt == m_parsedKeyValHash.end()){
        valWithMeta.rawStr = QString();
        valWithMeta.insertDefault = insertDefaultIfNotExist;
    } else {
        valWithMeta.rawStr = parsedValIt->second;
    }
    return valWithMeta;
}

void qsimplecfg::Section::setSectionName(const QString &sectionName)
{
    m_sectionName = sectionName;
}

const QString& qsimplecfg::Section::sectionName() const
{
    return m_sectionName;
}


/// Rename a parsed key. Warning: it is *not* allowed, to call this function
/// after having accessed a key via getValue(), because that would destroy
/// the order of the keys. So call this function after Cfg::parse but before
/// accessing any value.
bool qsimplecfg::Section::renameParsedKey(const QString &oldName, const QString &newName)
{
    assert(m_keyValHash.empty());
    auto oldIt = m_parsedKeyValHash.find(oldName);
    if(oldIt == m_parsedKeyValHash.end()){
        return false;
    }
    const QString value = oldIt->second;
    m_parsedKeyValHash[newName] = value;
    m_parsedKeyValHash.erase(oldIt);
    return true;
}


/// return those keys which were not read via getValue[s]() after insert();
const std::unordered_set<QString>& qsimplecfg::Section::notReadKeys() const
{
    return m_NotReadKeys;
}



