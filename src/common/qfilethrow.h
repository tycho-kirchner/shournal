#pragma once

#include <QFile>

class QFileThrow : public QFile
{
public:
    using QFile::QFile;

    bool open(QFile::OpenMode flags) override;
    bool open(int fd, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);
    qint64 write(const QByteArray &data, bool throwIfNotAllWritten=true);
};
