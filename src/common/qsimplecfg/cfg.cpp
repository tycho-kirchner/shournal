#include <cassert>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QLockFile>

#include "cfg.h"
#include "exccfg.h"
#include "util.h"
#include "qformattedstream.h"
#include "cflock.h"
#include "excos.h"

namespace  {


void setStreamCommentMode(QFormattedStream& s){
    s.setMaxLineWidth(80);
    s.setLineStart("# ");
}

void unsetStreamCommentMode(QFormattedStream& s){
    s.setMaxLineWidth(std::numeric_limits<int>::max());
    s.setLineStart("");
}

} // namespace


/// Parse the config file at filepath. Create it, if necessary.
/// Note thath the content of multi-line strings between triple quotes
/// is parsed "as is", except for an optional final \n, if the closing triple
/// quotes are in the next line. Example:
/// '''
/// text
/// '''
/// -> no \n after <text> (but before it there is one).
/// So it does not matter, wether the closing triple quotes are in the same
/// or the next line.
/// Another file in the same directory with _LOCK-extension is locked, before parsing.
/// @throws ExcCfg
void qsimplecfg::Cfg::parse(const QString &filepath)
{
    m_nameSectionHash.clear();
    m_initialComments.clear();
    m_lastFilepath = filepath;

    createDirsToFilename(filepath);
    QFile file(filepath);

    if(! file.open(QIODevice::OpenModeFlag::ReadWrite | QIODevice::OpenModeFlag::Text)){
        throw ExcCfg(qtr("Failed to open %1 - %2").
                     arg(filepath, file.errorString()));
    }
    CFlock lock(file.handle());
    try {
        lock.lockShared();
    } catch (const os::ExcOs& e) {
       throw ExcCfg(qtr("Parse error: failed to obtain lock on %1 : %2").arg(filepath, e.what()));
    }

    QTextStream in(&file);
    try{
        parse(&in);
    } catch(ExcCfg & ex){
        ex.setDescrip(ex.descrip() +
                       qtr(". Please correct the file at %1").arg(m_lastFilepath));
        throw;
    }
}

/// Save config at given filepath, or, if it is empty,
/// the one where it was loaded from via parse().
/// Another file in the same directory with _LOCK-extension is locked, before saving.
/// @throws ExcCfg
void qsimplecfg::Cfg::store(const QString &filepath)
{
    QFile file;
    if(! filepath.isEmpty()){
        file.setFileName(filepath);
    } else {
        if(m_lastFilepath.isEmpty()){
            throw QExcIllegalArgument("empty filepath passed while lastFilePath is also empty");
        }
        file.setFileName(m_lastFilepath);
    }

    createDirsToFilename(file.fileName());

    if(! file.open(QIODevice::OpenModeFlag::WriteOnly | QIODevice::OpenModeFlag::Text)){
        throw ExcCfg(qtr("Failed to open %1 - %2").
                     arg(filepath, file.errorString()));
    }
    CFlock lock(file.handle());
    try {
        lock.lockExclusive();
    } catch (const os::ExcOs& e) {
       throw ExcCfg(qtr("Store error: failed to obtain lock on %1 : %2").arg(filepath, e.what()));
    }

    QFormattedStream stream(&file);
    stream.setStreamChunkSep('\n');

    if(! m_initialComments.isEmpty()){
        setStreamCommentMode(stream);
        stream << m_initialComments;
        unsetStreamCommentMode(stream);
    }
    stream << "\n\n";

    for(auto it = m_nameSectionHash.begin(); it != m_nameSectionHash.end(); ++it){
        stream << '[' + it.key() + "]";
        if( ! it.value().comments().isEmpty()){
            setStreamCommentMode(stream);
            stream << it.value().comments();
            unsetStreamCommentMode(stream);
        }

        for(auto sectionIt = it.value().keyValHash().begin();
            sectionIt != it.value().keyValHash().end();
            ++sectionIt){
            QString optTriplesStart;
            QString optTriplesEnd;
            QString val = sectionIt.value().rawStr.trimmed();
            if(sectionIt.value().separator.contains('\n') ||
                    val.contains('\n')){
                optTriplesStart = "'''\n";
                optTriplesEnd = "\n'''";
            }
            stream << sectionIt.key() + " = " + optTriplesStart +
                                                 val +
                                                optTriplesEnd;

        }
        stream << "\n\n";
    }
}

qsimplecfg::Section &qsimplecfg::Cfg::operator[](const QString &key)
{
    auto sectIt = m_nameSectionHash.find(key);
    if(sectIt == m_nameSectionHash.end()){
        sectIt = m_nameSectionHash.insert(key, Section(key));
    }
    sectIt.value().setSectionWasRead(true);
    return sectIt.value();
}

const qsimplecfg::Cfg::SectionHash &qsimplecfg::Cfg::sectionHash()
{
    return m_nameSectionHash;
}


void qsimplecfg::Cfg::handleKeyValue(QStringRef &line, size_t *pLineNumber,
                                     QTextStream *stream, Section *section)
{
    int equalIdx = line.indexOf('=');
    if(equalIdx == -1){
        throw ExcCfg(qtr("Line %1 - %2: Unexpected content (missing =)").
                     arg(*pLineNumber).arg(line.toString()));
    }

    QStringRef key = line.left(equalIdx).trimmed();
    QStringRef value = line.mid(equalIdx + 1).trimmed();
    if(! value.startsWith("'''")){
        // simple case: not a multi line string
        section->insert(key.toString(), value.toString());
        return;
    }

    // ignore leading '''
    value = value.mid(3);
    // still possible that string ends in same line:
    int tripleIdx = value.indexOf("'''");
    if(tripleIdx != -1){
        if(tripleIdx != value.length()-3){
            throw ExcCfg(qtr("Line %1 - %2: content after closing triple quotes '''").
                         arg(*pLineNumber).arg(line.toString()));
        }
        section->insert(key.toString(), value.left(value.size() - 3).toString());
        return;
    }

    m_keyValBuf = value.toString();

    // mutli line string: keep going through file until the
    // next '''
    size_t startingLine = *pLineNumber;
    while (!stream->atEnd()) {
        if(! readLineInto(*stream, &m_keyValReadBuf)){
            break;
        }
        (*pLineNumber)++;
        QStringRef currentLine(&m_keyValReadBuf);
        currentLine = currentLine.trimmed();

        tripleIdx = currentLine.indexOf("'''");
        if(tripleIdx == -1){
            // keep \n's for later split
            m_keyValBuf += '\n' + currentLine.toString() ;
            continue;
        }
        if(tripleIdx != currentLine.length()-3){
            throw ExcCfg(qtr("Line %1 - %2: content after closing triple quotes '''").
                         arg(*pLineNumber).arg(currentLine.toString()));
        }
        if(tripleIdx != 0){            
            m_keyValBuf += '\n' + currentLine.left(currentLine.size() - 3).toString();
        }
        section->insert(key.toString(), m_keyValBuf);
        return;
    }
    throw ExcCfg(qtr("Line %1 - %2: missing closing triple quotes '''").
                 arg(startingLine).arg(line.toString()));


}

/// @throws ExcCfg
void qsimplecfg::Cfg::createDirsToFilename(const QString &filename)
{
    assert(! filename.isEmpty());
    QFileInfo fileInfo(filename);
    QDir dir;

    if(! dir.mkpath(fileInfo.absolutePath())){
        throw ExcCfg(qtr("Failed to create directories for path %1")
                                 .arg(fileInfo.absolutePath()) );
    }
}


void qsimplecfg::Cfg::setInitialComments(const QString &comments)
{
    m_initialComments = comments;
}

const QString& qsimplecfg::Cfg::lastFilePath() const
{
    return m_lastFilepath;
}

/// Return all sections and their keys which were not read after having been
/// inserted.
qsimplecfg::NotReadSectionKeys qsimplecfg::Cfg::generateNonReadSectionKeyPairs()
{
    NotReadSectionKeys allNotRead;
    for(auto it = m_nameSectionHash.begin(); it != m_nameSectionHash.end(); ++it){
        if(! it.value().sectionWasRead()){
            // section not of interest
            continue;
        }
       auto notReadKeys = it.value().notReadKeys();
       if(! notReadKeys.empty()){
           allNotRead.push_back({it.key(), notReadKeys});
       }
    }
    return allNotRead;
}

/// transfer all values of old section to new section and remove old.
/// Note that comments, etc. are not updated (because those are typically set already)
/// @return true, if both section existed
bool qsimplecfg::Cfg::moveValsToNewSect(const QString &oldName, const QString &newName)
{
    auto oldSectIt = m_nameSectionHash.find(oldName);
    auto newSectIt = m_nameSectionHash.find(newName);
    if(oldSectIt == m_nameSectionHash.end() ||
            newSectIt == m_nameSectionHash.end()){
        return false;
    }
    newSectIt.value().updateValuesFromOtherSection(oldSectIt.value());
    m_nameSectionHash.erase(oldSectIt);
    return true;
}

void qsimplecfg::Cfg::parse(QTextStream *in)
{
    bool withinSection=false;
    Section currentSection("DUMMY"); // will be overidden
    QString currentSectionName;
    size_t currentLine = 0;

    QString lineBuf;
    lineBuf.reserve(8192);
    while (!in->atEnd()) {
        if(! readLineInto(*in, &lineBuf)){
            break;
        }

        QStringRef line(&lineBuf);
        line = line.trimmed();
        currentLine++;

        if(line.startsWith('#')){
            ;
            // No point in reading comments.
        } else if(line.isEmpty()){
            ;
        } else if(line.startsWith('[')){
            if(! line.endsWith(']')){
                throw ExcCfg(qtr("Line %1 - %2: section start [ without closing end ] detected").
                             arg(currentLine).arg(line.toString()));
            }

            if(withinSection){
                // store previous section in map
                m_nameSectionHash.insert(currentSectionName, currentSection);
            }
            withinSection = true;
            currentSectionName = line.mid(1, line.size() - 2).toString();
            if(currentSectionName.isEmpty()){
                throw ExcCfg(qtr("Line %1 - %2: empty section detected").
                             arg(currentLine).arg(line.toString()));
            }
            currentSection = Section(currentSectionName);

        } else {
            if(! withinSection){
                throw ExcCfg(qtr("Line %1 - %2: Content before first section").
                             arg(currentLine).arg(line.toString()));
            }
            handleKeyValue(line, &currentLine, in, &currentSection );
        }
    }
    if(withinSection){
        // final section not added yet
        m_nameSectionHash.insert(currentSectionName, currentSection);
    }
}
