#pragma once

#include "logger.h"
#include "fileeventhandler.h"
#include "fanotify_controller.h"
#include "socket_message.h"
#include "fdcommunication.h"

class FanotifyController;
struct CommandInfo;

class FileWatcher {
public:
    FileWatcher();

    void setupShellLogger();

    [[noreturn]]
    void run();

    void setShellSessionUUID(const QByteArray &shellSessionUUID);

    void setArgv(char** argv, int argc);

    void setCommandEnvp(char **commandEnv);
    void setCommandFilename(char *commandFilename);

    void setSockFd(int sockFd);

    int sockFd() const;


private:
    struct MsenterChildReturnValue {
        MsenterChildReturnValue(pid_t p, int pipeWrite) :
            pid(p), pipeWriteEnd(pipeWrite){}
        pid_t pid;
        int pipeWriteEnd;
    };
    static const int RECEIVE_BUF_SIZE = 1024*1024;

    int m_sockFd;
    logger::LogRotate m_shellLogger;
    FileEventHandler m_fEventHandler;
    gid_t m_msenterGid;
    fdcommunication::SocketCommunication m_sockCom;
    QByteArray m_shellSessionUUID;
    int m_commandArgc;
    char* m_commandFilename;
    char **m_commandArgv;
    char ** m_commandEnvp;
    uid_t m_realUid;
    fdcommunication::SocketCommunication::Messages m_sockMessages;

    MsenterChildReturnValue setupMsenterTargetChildProcess();
    socket_message::E_SocketMsg pollUntilStopped(CommandInfo& cmdInfo,
                                 FanotifyController& fanotifyCtrl);
    socket_message::E_SocketMsg processSocketEvent( CommandInfo& cmdInfo );
    void flushToDisk(CommandInfo& cmdInfo);


};


