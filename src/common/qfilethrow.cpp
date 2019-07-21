#include "qfilethrow.h"

#include "util.h"

/// @throws QExcIo
/// @return: *always* true, only bool because of 'override'
bool QFileThrow::open(QIODevice::OpenMode flags)
{
    if(! QFile::open(flags)){
        throw QExcIo(qtr("Failed to open %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
    return true;
}

bool QFileThrow::open(int fd, QIODevice::OpenMode ioFlags, QFileDevice::FileHandleFlags handleFlags)
{
    if(! QFile::open(fd, ioFlags, handleFlags)){
        throw QExcIo(qtr("Failed to open fd %1: %2")
                     .arg(fd).arg(this->errorString()));
    }
    return true;
}

/// @throws QExcIo
qint64 QFileThrow::write(const QByteArray &data, bool throwIfNotAllWritten)
{
    auto bytesWritten = QFile::write(data);
    if(bytesWritten == -1){
        throw QExcIo(qtr("Failed to write to file %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
    if(throwIfNotAllWritten && bytesWritten != data.size()){
        throw QExcIo(qtr("Unexpected written size for file %1 - "
                         "expected %2, actual: %3")
                     .arg(this->fileName()).arg(data.size()).arg(bytesWritten));
    }
    return bytesWritten;
}
