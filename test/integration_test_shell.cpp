
#include <QDebug>

#include "autotest.h"
#include "qoutstream.h"
#include "util.h"
#include "osutil.h"
#include "helper_for_test.h"
#include "database/db_connection.h"
#include "database/db_controller.h"
#include "database/file_query_helper.h"

using subprocess::Subprocess;

namespace  {

void writeLine(int fd, const std::string& line){
    os::write(fd, line + "\n");
}

/// @return write-end of the pipe passed to the shell-process
int callWithRedirectedStdin(Subprocess& proc){
    int oldStdIn = os::dup(STDIN_FILENO);
    auto pipe_ = os::pipe(0);
    os::dup2(pipe_[0], STDIN_FILENO);
    os::close(pipe_[0]);

    proc.call(AutoTest::globals().integrationShellArgs);
    // restore stdin
    os::dup2(oldStdIn, STDIN_FILENO);
    os::close(oldStdIn);

    return pipe_[1];
}

}


class IntegrationTestShell : public QObject {
    Q_OBJECT

private:
    void cmdFileCheck(const std::string& cmd, const std::string& fpath ){
        testhelper::deletePaths();

        Subprocess proc;
        int writeFd = callWithRedirectedStdin(proc);

        // Do not pollute the history...
        // TODO: evtl. adjust this for other shells...
        std::string tmpHistCmd = "export HISTFILE=" + splitAbsPath(fpath).first + "/tmpHistfile";
        writeLine(writeFd, tmpHistCmd);
        writeLine(writeFd, "SHOURNAL_ENABLE");

        writeLine(writeFd, cmd);
        writeLine(writeFd, "exit 123");

        os::close(writeFd);
        QCOMPARE(proc.waitFinish(), 123);
        // Sleep a bit, to wait for the asynchronous shournal-run to finish
        usleep(100* 1000);

        auto dbCleanup = finally([] { db_connection::close(); });
        SqlQuery query;
        file_query_helper::addWrittenFileSmart(query, QString::fromStdString(fpath));
        auto cmdIter = db_controller::queryForCmd(query);
        QVERIFY(cmdIter->next());
        QCOMPARE(cmdIter->value().text, QString::fromStdString(cmd));

    }

private slots:
    void testIt() {
        auto pTmpDir = testhelper::mkAutoDelTmpDir();
        std::string tmpDirPath = pTmpDir->path().toStdString();

        std::string filepath = tmpDirPath + "/f1";
        std::vector<std::string> cmds {
                    "echo '%' > " + filepath, // percent unveiled a printf format bug in shournal 0.7
                    "(echo foo2 ) > " + filepath,
                    "(echo foo3 > " + filepath + ")",
                    "/bin/echo foo4 > " + filepath,
                    "sh -c 'echo foo5 > " + filepath + "'",
                    "echo foo6 > " + filepath + " & wait",
                    "(echo foo7 & wait ) > " + filepath,
                    "(echo foo8 > " + filepath + ") & wait",
                    "/bin/echo foo9 > " + filepath + " & wait",
                    "sh -c 'echo foo10 > " + filepath + " & wait'",
        };

        for(const auto& cmd : cmds){
            cmdFileCheck(cmd, filepath);
        }
    }




};

DECLARE_TEST(IntegrationTestShell)

#include "integration_test_shell.moc"
