#pragma once

#include <QVector>

#include "exccommon.h"


namespace fdcommunication {


class ExcFdComm : public QExcCommon
{
public:
    using QExcCommon::QExcCommon;
};


class SocketCommunication
{
public:
    struct Message{
        /// message-types < 0 are for internal use: -1 indicates an empty message
        /// (end of data: all instances of the other endpoint were closed).
        Message(int mesgType=-1,
                const QByteArray& b=QByteArray(),
                int fd = -1) : msgId(mesgType), bytes(b), fd(fd) {}

        bool operator==(const Message &rhs) const {
            return this->msgId == rhs.msgId &&
                    this->bytes == rhs.bytes &&
                    this->fd == rhs.fd;
        }

        int msgId;
        QByteArray bytes;
        int fd;
    };
    typedef QVector<Message> Messages;

    SocketCommunication();

    int sockFd() const;
    void setSockFd(int sockFd);
    void setReceiveBufferSize(int s);
    void setReceiveFdSize(int s);

    Messages receiveMessages();
    void receiveMessages(Messages* messages);

    void sendMsg(const Message& message);
    void sendMessages(const Messages& messages);

private:
    QByteArray m_receiveBuf;
    QByteArray m_receiveCtrlMsgBuf;
    int m_sockFd;
};


} // namespace fdcommunication




