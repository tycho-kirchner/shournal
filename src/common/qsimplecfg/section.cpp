


#include "section.h"
#include "user_str_conversions.h"

qsimplecfg::Section::Section(const QString &sectionName) :
    m_insertDefaultToComments(true),
    m_sectionWasRead(false),
    m_sectionName(sectionName)
{}


void qsimplecfg::Section::insert(const QString &key, const QString &value)
{
    ValueWithMeta v;
    v.rawStr=value;
    m_keyValHash.insert(key, v);
    m_NotReadKeys.insert(key);
}

qint64 qsimplecfg::Section::getFileSize(const QString &key, const qint64 &defaultValue,
                                        bool insertDefaultIfNotExist)
{
    try {
        UserStrConversions userStrConv;

        return userStrConv.bytesFromHuman( this->getValue<QString>(
                                        key,
                                        userStrConv.bytesToHuman(defaultValue),
                                        insertDefaultIfNotExist));
    } catch (const ExcUserStrConversion& ex) {
        throw qsimplecfg::ExcCfg(ex.descrip() + " (key " + key + ')' );
    }
}


const OrderedMap<QString, qsimplecfg::Section::ValueWithMeta> &qsimplecfg::Section::keyValHash()
{
    return m_keyValHash;
}


void qsimplecfg::Section::setComments(const QString &comments)
{
    m_comments = comments;
}

const QString& qsimplecfg::Section::comments() const
{
    return m_comments;
}

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

const QString& qsimplecfg::Section::sectionName() const
{
    return m_sectionName;
}

void qsimplecfg::Section::setSectionWasRead(bool sectionWasRead)
{
    m_sectionWasRead = sectionWasRead;
}

/// update the values of all existing keys with the *same* name from
/// other, if exist.
void qsimplecfg::Section::updateValuesFromOtherSection(const qsimplecfg::Section &other)
{
    for( auto oIt = other.m_keyValHash.begin(); oIt != other.m_keyValHash.end(); ++oIt){
        auto it = m_keyValHash.find(oIt.key());
        if(it == m_keyValHash.end()){
            // we don't have that key. Ignore it
            continue;
        }
        it.value() = oIt.value();
    }
}

bool qsimplecfg::Section::sectionWasRead() const
{
    return m_sectionWasRead;
}

/// return those keys which were not read via getValue[s]() after insert();
const std::unordered_set<QString>& qsimplecfg::Section::notReadKeys() const
{
    return m_NotReadKeys;
}



