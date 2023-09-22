#pragma once

#include <QFile>

class QFileThrow : public QFile
{
public:
    using QFile::QFile;

    void flush();

    bool open(QFile::OpenMode flags) override;
    bool open(FILE *f, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);
    bool open(int fd, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);

    bool seek(qint64 offset) override;

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 readLineData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

};
