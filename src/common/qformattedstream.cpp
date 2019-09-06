#include "qformattedstream.h"

#include "util.h"

QFormattedStream::QFormattedStream(QString *string, QIODevice::OpenMode openMode) :
    m_textStream(string, openMode)
{
    this->initCommon();
}

QFormattedStream::QFormattedStream(FILE *fileHandle, QIODevice::OpenMode openMode) :
    m_textStream(fileHandle, openMode)
{
    this->initCommon();
}

QFormattedStream::QFormattedStream(QIODevice *device) :
    m_textStream(device)
{
    this->initCommon();
}

QFormattedStream::QFormattedStream(QByteArray *array, QIODevice::OpenMode openMode) :
    m_textStream(array, openMode)
{
    this->initCommon();
}

QFormattedStream::QFormattedStream(const QByteArray &array, QIODevice::OpenMode openMode) :
    m_textStream(array, openMode)
{
    this->initCommon();
}


void QFormattedStream::initCommon()
{
    m_colNChars = 0;
    m_maxLineWidth = std::numeric_limits<int>::max();
    m_autoSepStreamChunks = true;
    m_streamChunkSep = ' ';
}


QFormattedStream &QFormattedStream::operator<<(const QString &str)
{
    return (*this)<<(QStringRef(&str));
}


QFormattedStream &QFormattedStream::operator<<(const QStringRef &str) {
    int wordStartIdx = -1;
    for(int i=0; i < str.size(); i++){
        const QChar& c = str.at(i);
        if(m_colNChars == 0){
           writeLineStart();
        }
        if(c.isSpace()){
            if(wordStartIdx != -1){
                QStringRef word = str.mid(wordStartIdx, i - wordStartIdx);
                handleWordEnd(word);
                wordStartIdx = -1;
            }
            writeSpace(c);
        } else {
            if(wordStartIdx == -1){
                wordStartIdx = i;
            }
        }
    }
    // Final word might not be written yet.
    // Note that word-breaks spreading over multiple strings (multiples calls of operator<<)
    // are not correctly handled, if autoSepWords is false.
    if(wordStartIdx != -1){
        QStringRef word = str.mid(wordStartIdx);
        handleWordEnd(word);
    }
    // if at beginning of line, words are already separated,
    // so don't write space in that case
    if(m_autoSepStreamChunks && m_colNChars != 0){
        writeSpace(m_streamChunkSep);
    }
    return *this;
}


/// Each line in the stream will start with the given string (also applies
/// to the first line)
void QFormattedStream::setLineStart(const QString &lineStart)
{
    m_lineStart = lineStart;
}

/// Latest after that many characters a word-conscious line-break is
/// performed. If a word is longer than maxLineWidth, it will be splittet,
/// so it fits into the minimum possible number of lines.
void QFormattedStream::setMaxLineWidth(int maxLineWidth)
{
    m_maxLineWidth = maxLineWidth;
}


void QFormattedStream::writeLineStart()
{
    m_textStream << m_lineStart;
    m_colNChars = m_lineStart.size();
}

void QFormattedStream::handleWordEnd(const QStringRef &word)
{
    // Check if it fits in current line.
    if(m_colNChars + word.size() > m_maxLineWidth){
        if(word.size() + m_lineStart.size() <= m_maxLineWidth){
            // write it to next line
            m_textStream << "\n";
            writeLineStart();
            m_textStream << word;
            m_colNChars += word.size();
        } else {
            writeLongWord(word);
        }
    } else {
        m_textStream << word;
        m_colNChars += word.size();
    }
}

/// If a word is too large to fit into one line,
/// print as much into each line as possible.
void QFormattedStream::writeLongWord(const QStringRef &word)
{
    // dont use stl-style iterator for compatability with qt-version < 5.4
    for(int i=0; i < word.size(); i++){
        const QChar c = word.at(i);
        if(m_colNChars >= m_maxLineWidth){
            m_textStream << "\n";
            writeLineStart();
        }
        m_textStream << c;
        m_colNChars++;
    }
}

/// If we are at end of desired width,
/// always write line feed.
void QFormattedStream::writeSpace(const QChar &c)
{
    if(c == QChar::LineFeed || m_colNChars >= m_maxLineWidth){
        m_textStream << "\n";
        m_colNChars = 0;
    } else {
        m_textStream << c;
        m_colNChars++;
    }
}


const QString &QFormattedStream::lineStart() const
{
    return m_lineStart;
}

int QFormattedStream::maxLineWidth() const
{
    return m_maxLineWidth;
}

QChar QFormattedStream::streamChunkSep() const
{
    return m_streamChunkSep;
}

void QFormattedStream::setStreamChunkSep(const QChar &streamChunkSep)
{
    m_streamChunkSep = streamChunkSep;
}


