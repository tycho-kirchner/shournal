#pragma once

#include <QFile>

class QFileThrow : public QFile
{
public:
    using QFile::QFile;

    void flush();

    bool open(QFile::OpenMode flags) override;
    bool open(int fd, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);

    bool seek(qint64 offset) override;

    qint64 write(const QByteArray &data, bool throwIfNotAllWritten=true);
};
