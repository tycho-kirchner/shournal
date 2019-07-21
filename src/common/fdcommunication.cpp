
#include <cassert>
#include <sys/socket.h>



#include "fdcommunication.h"
#include "os.h"
#include "cleanupresource.h"
#include "util.h"


using namespace fdcommunication;


struct MessageHeader {
    int msgId;
    size_t len; // length of custom payload
    bool containsFd;
};

static_assert (std::is_pod<MessageHeader>(), "");



SocketCommunication::SocketCommunication() : m_sockFd(-1)
{}


/// Block until we receive a message from the other endpoint of the set socket.
/// Make sure, the internal receive buffer is large enough
/// (* it most not be empty*).
void SocketCommunication::receiveMessages(Messages *messages){
    messages->clear();
    if(m_receiveCtrlMsgBuf.size() < int(CMSG_SPACE(sizeof(int)))){
        m_receiveCtrlMsgBuf.resize(CMSG_SPACE(sizeof(int)));
    }

    assert(! m_receiveBuf.isEmpty());

    iovec ioVector = { m_receiveBuf.data(), static_cast<size_t>(m_receiveBuf.size()) };

    struct msghdr msgHdr{};

    msgHdr.msg_iov = &ioVector;
    msgHdr.msg_iovlen = 1;
    msgHdr.msg_control = m_receiveCtrlMsgBuf.data();
    msgHdr.msg_controllen = size_t(m_receiveCtrlMsgBuf.size());

    size_t len = os::recvmsg(m_sockFd, &msgHdr);
    if (len == 0) {
        messages->push_back(-1);
        return;
    }

    if(len < sizeof (MessageHeader)){
        throw ExcFdComm(qtr("Bad socket message received (too small)"));
    }

    char* pData = m_receiveBuf.data();
    const char* finalpData = pData + len;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgHdr);


    int* currentFd = reinterpret_cast<int*>(CMSG_DATA(cmsg));
    while(true){
        auto *customHeader = reinterpret_cast<MessageHeader*>(pData);
        // Consume the header, find out the payload-length...
        pData += sizeof (MessageHeader);
        assert(pData <= finalpData);
        QByteArray payload(pData, static_cast<int>(customHeader->len));
        Message message(customHeader->msgId, payload);

        if (customHeader->containsFd) {
            // we received an fd via SCM_RIGHTS.
            // see also man 3 cmsg            
            assert(cmsg->cmsg_level == SOL_SOCKET &&
                   cmsg->cmsg_type  == SCM_RIGHTS);
            message.fd = *currentFd;
            ++currentFd;
        }

        messages->push_back(message);
        // ... consume the payload
        pData += customHeader->len;
        assert(pData <= finalpData);
        if(pData >= finalpData){
            break;
        }

    }
}


void SocketCommunication::sendMsg(const SocketCommunication::Message &message)
{
    this->sendMessages({message});
}

void SocketCommunication::sendMessages(const SocketCommunication::Messages &messages)
{
    QVector<iovec> iovects;
    iovects.reserve(messages.size() * 2);

    QVector<MessageHeader> headers;
    headers.reserve(messages.size());

    QVector<int> fds;
    for(const auto& msg : messages){
        assert(msg.msgId >= 0);

        // brace-initialze MessageHeader{}, otherwise valgrind complains (unitialized)
        headers.push_back(MessageHeader{});
        headers.last().msgId = msg.msgId;
        headers.last().len = size_t(msg.bytes.length());
        headers.last().containsFd = (msg.fd != -1) ;

        iovects.push_back({&headers.last(), sizeof (MessageHeader)});

        iovec io;
        io.iov_base = const_cast<void*>(static_cast<const void*>(msg.bytes.data()));
        io.iov_len = size_t(msg.bytes.size());
        iovects.push_back(io);

        if(msg.fd != -1){
            fds.push_back(msg.fd);
        }
    }
    assert(iovects.capacity() == iovects.size());
    assert(headers.capacity() == headers.size());


    struct msghdr messageHeader{};
    messageHeader.msg_iov = iovects.data();
    messageHeader.msg_iovlen = iovects.size();


    QByteArray buf( int(CMSG_SPACE(size_t(fds.size()) * sizeof(int)) ), '\0');

    if(! fds.isEmpty()){
        messageHeader.msg_control = buf.data();
        messageHeader.msg_controllen = size_t(buf.size());
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&messageHeader);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        int *fdptr = reinterpret_cast<int*>(CMSG_DATA(cmsg) );
        memcpy(fdptr, fds.data(), fds.size() * sizeof(int));
    }
    os::sendmsg(m_sockFd, &messageHeader);
}

int SocketCommunication::sockFd() const
{
    return m_sockFd;
}

void SocketCommunication::setSockFd(int fd)
{
    m_sockFd = fd;
}

void SocketCommunication::setReceiveBufferSize(int s)
{
    m_receiveBuf.resize(s);
}

/// set size of file descriptor buffer
void SocketCommunication::setReceiveFdSize(int s)
{
    m_receiveCtrlMsgBuf.resize( s * int(CMSG_SPACE(sizeof(int))) );
}

SocketCommunication::Messages SocketCommunication::receiveMessages()
{
    Messages messages;
    receiveMessages(&messages);
    return messages;
}





