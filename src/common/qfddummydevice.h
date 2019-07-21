#pragma once

#include <QIODevice>


/// Dummy wrapper because QFile::open(int fd,...) cannot handle an already
/// open fd...
class QFdDummyDevice : public QIODevice
{
public:
    QFdDummyDevice(int fd);

protected:
    int m_fd;
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
};
