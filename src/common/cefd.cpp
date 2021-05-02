
#include <sys/eventfd.h>

#include "cefd.h"
#include "os.h"
#include "excos.h"
#include "osutil.h"


CEfd::CEfd()
{
    m_fd = eventfd(0, EFD_CLOEXEC);
    if (m_fd == -1){
        throw os::ExcOs("eventfd failed");
    }
}

CEfd::~CEfd()
{
    teardown();
}

void CEfd::sendMsg(uint64_t n)
{
    os::write(m_fd, &n, sizeof (n));
}

uint64_t CEfd::recvMsg()
{
    uint64_t n;
    // Block until parent process did the setup
    if(os::read(m_fd, &n, sizeof(n)) != sizeof(n)){
        throw os::ExcOs("cefd: read wrong size.");
    }
    return n;
}

void CEfd::teardown()
{
    if(m_fd != -1){
        osutil::closeVerbose(m_fd);
        m_fd = -1;
    }
}
