#include "qfilethrow.h"

#include "util.h"

/// @throws QExcIo
void QFileThrow::flush()
{
    if(! QFile::flush()){
        throw QExcIo(qtr("Failed to flush %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
}

bool QFileThrow::open(QIODevice::OpenMode flags)
{
    if(! QFile::open(flags)){
        throw QExcIo(qtr("Failed to open %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
    return true;
}

bool QFileThrow::open(FILE *f, QIODevice::OpenMode ioFlags, QFileDevice::FileHandleFlags handleFlags)
{
    if(! QFile::open(f, ioFlags, handleFlags)){
        throw QExcIo(qtr("Failed to open file: %1")
                     .arg(this->errorString()));
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
/// @return: *always* true, only bool because of 'override'
bool QFileThrow::seek(qint64 offset)
{
    if(! QFile::seek(offset)){
        throw QExcIo(qtr("Failed to seek %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
    return true;
}


qint64 QFileThrow::read(char *data, qint64 maxSize){
    auto ret = QFile::read(data, maxSize);
    if(ret == -1){
        throw QExcIo(qtr("Failed to read from file %1: %2")
                     .arg(this->fileName(), this->errorString()));
    }
    return ret;
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


