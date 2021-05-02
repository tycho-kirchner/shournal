#pragma once

#include <QByteArray>
#include <memory>

#include "commandinfo.h"
#include "mark_helper.h"

extern const pid_t INVALID_PID;

struct shournalk_group;
class CEfd;

class Filewatcher_shournalk
{
public:
    typedef std::shared_ptr<ShournalkControl> ShournalK_ptr;

public:
    static QByteArray fifopathForPid(pid_t pid);

    Filewatcher_shournalk();

    void setArgv(char **argv, int argc);
    void setPid(const pid_t &pid);

    void setCommandFilename(char *commandFilename);
    void setStoreToDatabase(bool storeToDatabase);

    void setShellSessionUUID(const QByteArray &shellSessionUUID);
    void setForkIntoBackground(bool value);
    void setCmdString(const QString &cmdString);
    void setFifoname(const QByteArray &fifoname);
    void setPrintSummary(bool printSummary);

    [[noreturn]]
    void run();


private:
    CommandInfo runExec(ShournalK_ptr& shournalk, CEfd &toplvlEfd);
    CommandInfo runMarkPid(ShournalK_ptr& shournalk, CEfd &toplvlEfd);


    int m_commandArgc{};
    char* m_commandFilename{};
    char **m_commandArgv;
    bool m_forkIntoBackground{};
    pid_t m_pid{INVALID_PID};
    bool m_printSummary{false};
    bool m_storeToDatabase{true};
    QByteArray m_shellSessionUUID;
    QString m_cmdString;
    QByteArray m_fifoname;



};

