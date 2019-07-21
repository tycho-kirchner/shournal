#pragma once

#include <QTextStream>


/// Write strings to text-streams with a custom formatting.
/// Each line can be set to start with an arbitrary string.
/// If maxLineWidth is set, split a string word-aware once
/// a line becomes too long.
/// Strings received during multiple <<-operator-calls are
/// automatically separated by whitespace (or the desired char), if not already separated
/// by a character for which QChar::isSpace() returns true.
/// Note: Avoid using the tab-character as its width is controlled by the terminal.
class QFormattedStream
{
public:
    QFormattedStream(QString *string, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    QFormattedStream(FILE *fileHandle, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    QFormattedStream(QIODevice *device);
    QFormattedStream(QByteArray *array, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    QFormattedStream(const QByteArray &array, QIODevice::OpenMode openMode = QIODevice::ReadOnly);

    QFormattedStream& operator<<(const QString& str);
    QFormattedStream& operator<<(const QStringRef& str);

    void setLineStart(const QString &lineStart);
    void setMaxLineWidth(int maxLineWidth);
    void setStreamChunkSep(const QChar &streamChunkSep);

private:
    void initCommon();
    void writeLineStart();
    void handleWordEnd(const QStringRef &word);
    void writeLongWord(const QStringRef &word);
    void writeSpace(const QChar& c);

    QTextStream m_textStream;
    QString m_lineStart;
    int m_colNChars; // number of written characters in current line
    int m_maxLineWidth;
    bool m_autoSepStreamChunks;
    QChar m_streamChunkSep;
};
