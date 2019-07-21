
// Some older systems like CentOS7 (older glibc-versions) don't provide it yet.
// Below workaround is ugly but works.
#if __has_include(<linux/kcmp.h>)
#include <linux/kcmp.h>
#else
enum kcmp_type { KCMP_FILE};
#endif

#include <sys/syscall.h>

#include <sys/socket.h>
#include <QTest>
#include <QTemporaryFile>


#include "autotest.h"
#include "fdcommunication.h"
#include "os.h"
#include "cleanupresource.h"
#include "excos.h"

using fdcommunication::SocketCommunication;
using Message = SocketCommunication::Message;


static QPair<SocketCommunication, SocketCommunication> makeSockets(){
    auto sockets = os::socketpair(PF_UNIX, SOCK_STREAM);
    SocketCommunication sendSock;
    sendSock.setSockFd(sockets[0]);

    SocketCommunication receiveSock;
    receiveSock.setReceiveBufferSize(1024);
    receiveSock.setSockFd(sockets[1]);

    return {sendSock, receiveSock};
}

class FCommunicationTest : public QObject {
    Q_OBJECT
private:
    bool fdsAreEqual(int fd1, int fd2)
    {
        auto pid = getpid();
        auto ret =  syscall(SYS_kcmp, pid, pid, KCMP_FILE, fd1, fd2);
        if(ret == -1)throw os::ExcOs("SYS_kcmp failed: ");
        return ret == 0;
    }


private slots:

    void tNormal() {
        auto sockets = makeSockets();
        auto sendSock = sockets.first;
        auto receiveSock = sockets.second;
        auto closeSocks = finally([&sendSock, &receiveSock] {
            close(sendSock.sockFd());
            close(receiveSock.sockFd());
        });

        Message msg1{1,  "echo hi some_text_with_umläüts and greek "
                         "δ ω π Δ σ α β γ Σ λ ε µ DONE" };
        sendSock.sendMsg(msg1);

        Message msg2{2, "abcdefg"};
        sendSock.sendMsg(msg2);

        auto messages = receiveSock.receiveMessages();

        QCOMPARE(2, messages.size());
        QCOMPARE(msg1, messages[0]);
        QCOMPARE(msg2, messages[1]);

        // send both messages aggregated.
        sendSock.sendMessages({msg1, msg2});

        QCOMPARE(2, messages.size());
        QCOMPARE(msg1, messages[0]);
        QCOMPARE(msg2, messages[1]);
    }


    void tFd() {
        auto sockets = makeSockets();
        auto sendSock = sockets.first;
        auto receiveSock = sockets.second;
        auto closeSocks = finally([&sendSock, &receiveSock] {
            close(sendSock.sockFd());
            close(receiveSock.sockFd());
        });

        QTemporaryFile tmpFile1;
        QVERIFY(tmpFile1.open());

        Message msg1{1, "foobar", tmpFile1.handle()};

        sendSock.sendMsg(msg1);


        auto messages = receiveSock.receiveMessages();

        QCOMPARE(1, messages.size());
        QCOMPARE(msg1.msgId, messages[0].msgId);
        QCOMPARE(msg1.bytes, messages[0].bytes);
        QVERIFY(messages[0].fd != -1);

        QVERIFY(fdsAreEqual(msg1.fd, messages[0].fd));
        os::close(messages[0].fd);


        // check two fds in sequence.
        // In this case they are also received in sequence.
        receiveSock.setReceiveFdSize(10);
        QTemporaryFile tmpFile2;
        QVERIFY(tmpFile2.open());

        Message msg2{2, "ok_youä#ü", tmpFile2.handle()};


        sendSock.sendMsg(msg1);
        sendSock.sendMsg(msg2);

        messages = receiveSock.receiveMessages();
        QCOMPARE(1, messages.size());
        QCOMPARE(msg1.msgId, messages[0].msgId);
        QCOMPARE(msg1.bytes, messages[0].bytes);
        QVERIFY(messages[0].fd != -1);

        QVERIFY(fdsAreEqual(msg1.fd, messages[0].fd));
        os::close(messages[0].fd);

        messages = receiveSock.receiveMessages();
        QCOMPARE(1, messages.size());
        QCOMPARE(msg2.msgId, messages[0].msgId);
        QCOMPARE(msg2.bytes, messages[0].bytes);
        QVERIFY(messages[0].fd != -1);

        QVERIFY(fdsAreEqual(msg2.fd, messages[0].fd));
        os::close(messages[0].fd);


        // test two fds at once
        sendSock.sendMessages({msg1, msg2});
        messages = receiveSock.receiveMessages();
        QCOMPARE(2, messages.size());
        QCOMPARE(msg1.msgId, messages[0].msgId);
        QCOMPARE(msg1.bytes, messages[0].bytes);
        QVERIFY(messages[0].fd != -1);
        QCOMPARE(msg2.msgId, messages[1].msgId);
        QCOMPARE(msg2.bytes, messages[1].bytes);
        QVERIFY(messages[1].fd != -1);

        QVERIFY(fdsAreEqual(msg1.fd, messages[0].fd));
        os::close(messages[0].fd);

        QVERIFY(fdsAreEqual(msg2.fd, messages[1].fd));
        os::close(messages[1].fd);


        // test two fds with regular message (without fd) in between

        Message msgReg(3, "reg");
        sendSock.sendMessages({msg1, msgReg, msg2});

        messages = receiveSock.receiveMessages();
        QCOMPARE(3, messages.size());
        QCOMPARE(msg1.msgId, messages[0].msgId);
        QCOMPARE(msg1.bytes, messages[0].bytes);
        QVERIFY(messages[0].fd != -1);

        QVERIFY(fdsAreEqual(msg1.fd, messages[0].fd));
        os::close(messages[0].fd);

        QCOMPARE(msgReg, messages[1]);

        QCOMPARE(msg2.msgId, messages[2].msgId);
        QCOMPARE(msg2.bytes, messages[2].bytes);
        QVERIFY(messages[2].fd != -1);
        QVERIFY(fdsAreEqual(msg2.fd, messages[2].fd));
        os::close(messages[2].fd);

 }

};


DECLARE_TEST(FCommunicationTest)

#include "test_fdcommunication.moc"
