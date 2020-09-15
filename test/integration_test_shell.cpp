
#include <QDebug>

#include "autotest.h"
#include "qoutstream.h"
#include "util.h"
#include "osutil.h"
#include "helper_for_test.h"
#include "database/db_connection.h"
#include "database/db_controller.h"
#include "database/file_query_helper.h"
#include "database/query_columns.h"
#include "qsimplecfg/cfg.h"
#include "settings.h"
#include "database/storedfiles.h"

using subprocess::Subprocess;
using db_controller::QueryColumns;

namespace  {

void writeLine(int fd, const std::string& line){
    os::write(fd, line + "\n");
}

os::Pipes_t prepareHighFdNumberPipe(){
    auto pipe_ = os::pipe(0, false); // CLOEXEC irrelevant, dup2 below...
    int highFd = osutil::findHighestFreeFd();
    os::dup2(pipe_[0], highFd);
    os::close(pipe_[0]);
    pipe_[0] = highFd;

    highFd = osutil::findHighestFreeFd(highFd - 1);
    os::dup2(pipe_[1], highFd);
    os::close(pipe_[1]);
    pipe_[1] = highFd;
    os::setenv<QByteArray>("_SHOURNAL_INTEGRATION_TEST_PIPE_FD",
               QByteArray::number(pipe_[1]));

    return pipe_;
}


/// @return write-end of the pipe passed to the shell-process
int callWithRedirectedStdin(Subprocess& proc){
    int oldStdIn = os::dup(STDIN_FILENO); // CLOEXEC irrelevant, dup2 below...
    auto pipe_ = os::pipe(0);
    os::dup2(pipe_[0], STDIN_FILENO);
    os::close(pipe_[0]);

    proc.call(AutoTest::globals().integrationShellArgs);
    // restore stdin
    os::dup2(oldStdIn, STDIN_FILENO);
    os::close(oldStdIn);

    return pipe_[1];
}


} // anonymous namespace


class IntegrationTestShell : public QObject {
    Q_OBJECT

private:
    void writeReadSettingsToCfgFile(const QString& readIncludeDir){
        auto & sets = Settings::instance();
        qsimplecfg::Cfg cfg;
        auto sectRead = cfg[Settings::SECT_READ_NAME];
        sectRead->getValue(Settings::SECT_READ_KEY_ENABLE, true, true);
        sectRead->getValue(Settings::SECT_READ_KEY_INCLUDE_PATHS, readIncludeDir, true);
        auto cfgPath = sets.cfgFilepath();
        cfg.store(cfgPath);
    }

    void writeScriptSettingToCfgFile(const QString& includePath, const QStringList& fileExtensions){
        auto & sets = Settings::instance();
        qsimplecfg::Cfg cfg;
        auto sectRead = cfg[Settings::SECT_SCRIPTS_NAME];
        sectRead->getValue(Settings::SECT_SCRIPTS_ENABLE, true, true);
        sectRead->getValue(Settings::SECT_SCRIPTS_INCLUDE_PATHS, includePath, true);
        sectRead->getValues(Settings::SECT_SCRIPTS_INCLUDE_FILE_EXTENSIONS, fileExtensions, true, "\n");
        auto cfgPath = sets.cfgFilepath();
        cfg.store(cfgPath);
    }


    /// @param cmd: the command to be executed
    /// @param setupCommand: command executed before SHOURNAL_ENABLE
    void executeCmdInbservedShell(const std::string& cmd, const std::string& setupCommand){
        auto pipe_ = prepareHighFdNumberPipe();
        Subprocess proc;
        // pass pipe write end -> wait for async shournal grand-child process
        proc.setForwardFdsOnExec({pipe_[1]});
        int writeFd = callWithRedirectedStdin(proc);
        os::close(pipe_[1]);

        if(! setupCommand.empty()){
            writeLine(writeFd, setupCommand);
        }
        writeLine(writeFd, "SHOURNAL_ENABLE");
        writeLine(writeFd, "SHOURNAL_SET_VERBOSITY " +
                  std::string(logger::msgTypeToStr(logger::getVerbosityLevel())));

        writeLine(writeFd, cmd);
        writeLine(writeFd, "SHOURNAL_DISABLE");
        writeLine(writeFd, "exit 123");

        os::close(writeFd);
        QCOMPARE(proc.waitFinish(), 123);
        char c;
        // wait for shournal grand-child process to finish (close it's write end)
        os::read(pipe_[0], &c, 1);
        os::close(pipe_[0]);
    }

    /// @overload
    void executeCmdInbservedShell(const QString& cmd, const std::string& setupCommand){
        executeCmdInbservedShell(cmd.toStdString(), setupCommand);
    }



    void cmdWrittenFileCheck(const std::string& cmd, const std::string& fpath,
                             const std::string& setupCommand){
        executeCmdInbservedShell(cmd, setupCommand);
        SqlQuery query;
        file_query_helper::addWrittenFileSmart(query, QString::fromStdString(fpath));
        auto cmdIter = db_controller::queryForCmd(query);
        auto dbCleanup = finally([] { db_connection::close(); });
        QVERIFY(cmdIter->next());
        QCOMPARE(cmdIter->value().text, QString::fromStdString(cmd));
        QVERIFY(QFile(QString::fromStdString(fpath)).remove());
    }

private slots:

    void initTestCase(){
        logger::setup(__FILE__);
    }

    /// Called for each test.
    void init(){
        testhelper::deletePaths();
        // Load settings and delete the config-file. That way,
        // The version of the cfg-file is also set appropriately.
        Settings::instance().load();
        QFile(Settings::instance().cfgFilepath()).remove();
    }

    void cleanup(){

    }



    void testWrite() {
        auto pTmpDir = testhelper::mkAutoDelTmpDir();
        auto tmpDirPath = pTmpDir->path().toStdString();


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
                    // relative paths must also work:
                    "cd " + tmpDirPath + "; echo hi > f1",
                    "cd " + tmpDirPath + "; echo hi > ./f1",
                    "cd " + tmpDirPath + "; echo hi > ../" + splitAbsPath(tmpDirPath).second + "/f1",
        };

        const auto setupCmd = AutoTest::globals().integrationSetupCommand;

        for(const auto& cmd : cmds){
            testhelper::deleteDatabaseDir();
            cmdWrittenFileCheck(cmd, filepath, setupCmd);
        }
    }


    void testRead(){
        const auto setupCmd = AutoTest::globals().integrationSetupCommand;

        auto pTmpDir = testhelper::mkAutoDelTmpDir();
        // for read events only include our tempdir
        writeReadSettingsToCfgFile(pTmpDir->path());

        const QString fname = "foo1";
        const QString fullPath = pTmpDir->path() + '/' + fname;

        QFileThrow(fullPath).open(QFile::WriteOnly);
        const QString cmdTxt = "cat " + fullPath;
        executeCmdInbservedShell(cmdTxt, setupCmd);

        SqlQuery query;
        const auto & cols = QueryColumns::instance();

        query.addWithAnd(cols.rFile_path, pTmpDir->path());
        query.addWithAnd(cols.rFile_name, fname);

        auto cmdIter = db_controller::queryForCmd(query);
        auto dbCleanup = finally([] { db_connection::close(); });
        QVERIFY(cmdIter->next());
        auto cmdInfo = cmdIter->value();
        QCOMPARE(cmdInfo.text, cmdTxt);
        QCOMPARE(cmdInfo.fileReadInfos.size(), 1);
        const auto& fReadInfo = cmdInfo.fileReadInfos.first();
        QCOMPARE(fReadInfo.name, fname);
        QCOMPARE(fReadInfo.path, pTmpDir->path());
        QCOMPARE(fReadInfo.isStoredToDisk, false);

        QVERIFY(! cmdIter->next());
    }


    void testReadScript(){
        const auto setupCmd = AutoTest::globals().integrationSetupCommand;

        auto pTmpDir = testhelper::mkAutoDelTmpDir();
        // for read events only include our tempdir
        writeScriptSettingToCfgFile(pTmpDir->path(), {"sh"});

        const QString fname = "foo1.sh";
        const QString fullPath = pTmpDir->path() + '/' + fname;
        const QString content("abcdefg");
        testhelper::writeStringToFile(fullPath, content);

        const QString cmdTxt = "cat " + fullPath;
        executeCmdInbservedShell(cmdTxt, setupCmd);

        SqlQuery query;
        const auto & cols = QueryColumns::instance();

        query.addWithAnd(cols.rFile_path, pTmpDir->path());
        query.addWithAnd(cols.rFile_name, fname);

        auto cmdIter = db_controller::queryForCmd(query);
        auto dbCleanup = finally([] { db_connection::close(); });
        QVERIFY(cmdIter->next());
        auto cmdInfo = cmdIter->value();
        QCOMPARE(cmdInfo.text, cmdTxt);
        QCOMPARE(cmdInfo.fileReadInfos.size(), 1);
        const auto& fReadInfo = cmdInfo.fileReadInfos.first();
        QCOMPARE(fReadInfo.name, fname);
        QCOMPARE(fReadInfo.path, pTmpDir->path());
        QCOMPARE(fReadInfo.isStoredToDisk, true);
        StoredFiles storedFiles;
        const QString pathToFileInDb = StoredFiles::getReadFilesDir() + "/" +
                QString::number(fReadInfo.idInDb);
        QFileThrow fInDb(pathToFileInDb);
        QVERIFY(fInDb.exists());
        fInDb.open(QFile::ReadOnly);
        QCOMPARE(content, testhelper::readStringFromFile(fullPath));

        QVERIFY(! cmdIter->next());
    }





};

DECLARE_TEST(IntegrationTestShell)

#include "integration_test_shell.moc"
