
#pragma once

#include <cstdint>

#include "util.h"

class CEfd {
public:
    static const uint64_t MSG_OK {7};
    static const uint64_t MSG_FAIL {8};

    CEfd();
    ~CEfd();

    void sendMsg(uint64_t n);
    uint64_t recvMsg();

    void teardown();


private:
    Q_DISABLE_COPY(CEfd)
    DISABLE_MOVE(CEfd)

    int m_fd;
};
