
#pragma once

#include <QString>
#include <QHash>
#include <QStringList>

#include "section.h"

class QTextStream;

namespace qsimplecfg {

/// First Element of each pair is section-name, second a set of not read keys
typedef QVector<QPair<QString, std::unordered_set<QString> > > NotReadSectionKeys ;


/// A simple parser for ini-like config files.
/// No subsections are supported but:
/// - initial file comment
/// - comments after a section header:
///   [sectionname]
///   # comment1
///   # comment2
/// - values over multiple lines with triple quotes ''':
///   key = '''foo1
///         foo2'''
/// - comments are always re-written by the application on store(),
///   but ignored when parsing the file.
/// - The order of sections and keys is preserved
class Cfg
{
public:
    typedef OrderedMap<QString, Section> SectionHash;

    void parse(const QString& filepath);
    void store(const QString& filepath=QString());

    Section & operator[](const QString &key) ;

    const SectionHash& sectionHash();

    void setInitialComments(const QString &comments);

    const QString& lastFilePath() const;

    NotReadSectionKeys generateNonReadSectionKeyPairs();

    bool moveValsToNewSect(const QString& oldName, const QString& newName);

private:
    SectionHash m_nameSectionHash;
    QString m_initialComments;
    QString m_lastFilepath;
    QString m_keyValReadBuf;
    QString m_keyValBuf;

    void parse(QTextStream *in);

    void handleKeyValue(QStringRef &line,
                        size_t* pLineNumber,
                        QTextStream* stream,
                        Section* section);

    static void createDirsToFilename(const QString& filename);
};

} // namespace qsimplecfg
