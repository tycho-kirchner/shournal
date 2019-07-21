#include "qfddummydevice.h"

#include "os.h"

QFdDummyDevice::QFdDummyDevice(int fd) : m_fd(fd){}


qint64 QFdDummyDevice::readData(char *data, qint64 maxlen) {
    return os::read(m_fd, data,static_cast<size_t>(maxlen));
}

qint64 QFdDummyDevice::writeData(const char *data, qint64 len) {
    return os::write(m_fd, data, static_cast<size_t>(len));
}
