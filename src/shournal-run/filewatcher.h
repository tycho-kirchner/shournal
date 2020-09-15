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


    void setStoreToDatabase(bool storeToDatabase);

private:
    struct MsenterChildReturnValue {
        MsenterChildReturnValue(pid_t p, int pipeWrite) :
            pid(p), pipeWriteEnd(pipeWrite){}
        pid_t pid;
        int pipeWriteEnd;
    };
    typedef std::unique_ptr<FanotifyController> FanotifyController_ptr;

    static const int RECEIVE_BUF_SIZE = 1024*1024;

    int m_sockFd;
    logger::LogRotate m_shellLogger;
    std::shared_ptr<FileEventHandler> m_fEventHandler;
    gid_t m_msenterGid;
    fdcommunication::SocketCommunication m_sockCom;
    QByteArray m_shellSessionUUID;
    int m_commandArgc;
    char* m_commandFilename;
    char **m_commandArgv;
    char ** m_commandEnvp;
    uid_t m_realUid;
    fdcommunication::SocketCommunication::Messages m_sockMessages;
    bool m_storeToDatabase;

    MsenterChildReturnValue setupMsenterTargetChildProcess();
    socket_message::E_SocketMsg pollUntilStopped(CommandInfo& cmdInfo,
                                 FanotifyController_ptr& fanotifyCtrl);
    socket_message::E_SocketMsg processSocketEvent( CommandInfo& cmdInfo );
    void flushToDisk(CommandInfo& cmdInfo);


};


