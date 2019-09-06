#include <iostream>

#include "qfddummydevice.h"

#include "os.h"

/// @param becomeOwner: if true, close the fd in destructor
QFdDummyDevice::QFdDummyDevice(int fd, bool becomeOwner) :
    m_fd(fd),
    m_owner(becomeOwner)
{}

QFdDummyDevice::~QFdDummyDevice()
{
    if(m_owner){
        try {
            os::close(m_fd);
        } catch (const os::ExcOs& e) {
            std::cerr << __func__ << " " << e.what() << "\n";
        }
    }
}


qint64 QFdDummyDevice::readData(char *data, qint64 maxlen) {
    return os::read(m_fd, data,static_cast<size_t>(maxlen));
}

qint64 QFdDummyDevice::writeData(const char *data, qint64 len) {
    return os::write(m_fd, data, static_cast<size_t>(len));
}
