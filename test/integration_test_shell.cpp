
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
#include "safe_file_update.h"

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
    void writeReadSettingsToCfg(const QString& readIncludeDir,
                                qsimplecfg::Cfg& cfg){
        auto sectRead = cfg[Settings::SECT_READ_NAME];
        sectRead->getValue(Settings::SECT_READ_KEY_ENABLE, true, true);
        sectRead->getValue(Settings::SECT_READ_KEY_INCLUDE_PATHS, readIncludeDir, true);
    }

    void writeScriptSettingToCfg(const QString& includePath,
                                 const QStringList& fileExtensions,
                                 qsimplecfg::Cfg& cfg){
        auto sectRead = cfg[Settings::SECT_SCRIPTS_NAME];
        sectRead->getValue(Settings::SECT_SCRIPTS_ENABLE, true, true);
        sectRead->getValue(Settings::SECT_SCRIPTS_INCLUDE_PATHS, includePath, true);
        sectRead->getValues(Settings::SECT_SCRIPTS_INCLUDE_FILE_EXTENSIONS, fileExtensions, true, "\n");
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
        auto proc_ret = proc.waitFinish();
        if(proc_ret == 42){
            QIErr() << QString::fromUtf8("As of zsh 5.7.1 the fanotify shell integration backend "
                       "*must* be enabled during zsh-startup (e.g .zshrc) for the "
                       "integration-tests to succeed. Otherwise the first zsh-process "
                       "*may* consume stdin so no commands remain after «exec zsh» "
                       "which is called to preload libshournal-shellwatch.so. "
                       "See also my email 'Unexpected stdin-behavior' from 2021-10-21 "
                       "on the zsh mailing list zsh-workers@zsh.org. Note that zsh "
                       "does not follow posix shell behaviour here: "
                       "«When the shell is using "
                       "standard input and it invokes a command that also uses standard "
                       "input, the shell shall ensure that the standard input file "
                       "pointer points directly after the command it has read when "
                       "the command begins execution».");
            throw QExcIllegalArgument("Bad integration-test environment (see above)");
        }

        QCOMPARE(proc_ret, 123);
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
        auto query = file_query_helper::buildFileQuerySmart(
                    QString::fromStdString(fpath), false);
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
        QVERIFY(tmpDirPath != "/"); // otherwise this test must be changed
        auto tmpDirNoLeadingSlash(tmpDirPath);
        tmpDirNoLeadingSlash.erase(tmpDirNoLeadingSlash.begin());

        auto r510 = pTmpDir->path() + "/510";
        testhelper::writeStuffToFile(r510, 510);

        auto r4096 = pTmpDir->path() + "/4096";
        testhelper::writeStuffToFile(r4096, 4096);

        auto r21567 = pTmpDir->path() + "/21567";
        testhelper::writeStuffToFile(r21567, 21567);

        auto r101978 = pTmpDir->path() + "/101978";
        testhelper::writeStuffToFile(r101978, 101978);


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
                    // malformed filepath with multiple slash //
                    "echo foo11 > //" + filepath,
                    // special case root dir
                    "cd /; echo foo11 > //" + filepath,
                    // relative paths must also work:
                    "cd " + tmpDirPath + "; echo hi > f1",
                    "cd " + tmpDirPath + "; echo hi > ./f1",
                    "cd " + tmpDirPath + "; echo hi > ../" + splitAbsPath(tmpDirPath).second + "/f1",
                    // special case root dir
                    "cd /; echo hi > " + tmpDirNoLeadingSlash + "/f1",
                    // test also if partial hashing works for bigger files
                    "cat " + r510.toStdString() + " > " + filepath,
                    "cat " + r4096.toStdString() + " > " + filepath,
                    "cat " + r21567.toStdString() + " > " + filepath,
                    "cat " + r101978.toStdString() + " > " + filepath,
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
        qsimplecfg::Cfg cfg;
        writeReadSettingsToCfg(pTmpDir->path(), cfg);
        auto & sets = Settings::instance();
        SafeFileUpdate cfgUpd8(sets.cfgFilepath());
        cfgUpd8.write([&cfg, &cfgUpd8]{
           cfg.store(cfgUpd8.file());
        });


        const QString fname = "foo1";
        const QString fullPath = pTmpDir->path() + '/' + fname;

        QFileThrow(fullPath).open(QFile::WriteOnly);
        QString cmdTxt = "cat " + fullPath;
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
        auto fReadInfo = cmdInfo.fileReadInfos.first();
        QCOMPARE(fReadInfo.name, fname);
        QCOMPARE(fReadInfo.path, pTmpDir->path());
        QCOMPARE(fReadInfo.isStoredToDisk, false);

        QVERIFY(! cmdIter->next());
        cmdIter.reset();

        // Test exec (collected as read files). Copy system echo-binary to out tmppath
        auto echoPath = QStandardPaths::findExecutable("echo").toLocal8Bit();
        QVERIFY(! echoPath.isEmpty());
        auto echoInTmp = pathJoinFilename(pTmpDir->path().toLocal8Bit(),
                                          QByteArray("echo"));
        os::sendfile(echoInTmp, echoPath, os::stat(echoPath).st_size);
        os::chmod(echoInTmp, 0755);

        // Check if called binaries are tracked when directly called or indirectly, via
        // env.
        for(QString cmdTxt : QStringList{echoInTmp + " exec_trace_test",
                                         "env " + echoInTmp + " exec_trace_test"}){
            executeCmdInbservedShell(cmdTxt, setupCmd);
            query.clear();
            query.addWithAnd(cols.cmd_txt, cmdTxt);

            // Have to explicitly free cmdIter at loop-end to avoid database locks.
            // On first loop, the transaction of cmdIter may still be active, so
            // the database would be locked, when we execute the next command in
            // the shell (!).
            auto cmdIter = db_controller::queryForCmd(query);
            QVERIFY(cmdIter->next());
            cmdInfo = cmdIter->value();
            QCOMPARE(cmdInfo.text, cmdTxt);
            QCOMPARE(cmdInfo.fileReadInfos.size(), 1);
            fReadInfo = cmdInfo.fileReadInfos.first();
            QCOMPARE(fReadInfo.name, QString("echo"));
            QCOMPARE(fReadInfo.path, pTmpDir->path());
        }
    }


    void testReadScript(){
        const auto setupCmd = AutoTest::globals().integrationSetupCommand;

        auto pTmpDir = testhelper::mkAutoDelTmpDir();
        // for read events only include our tempdir
        qsimplecfg::Cfg cfg;
        writeScriptSettingToCfg(pTmpDir->path(), {"sh"}, cfg);
        writeReadSettingsToCfg(pTmpDir->path(), cfg);
        auto & sets = Settings::instance();
        SafeFileUpdate cfgUpd8(sets.cfgFilepath());
        cfgUpd8.write([&cfg, &cfgUpd8]{
           cfg.store(cfgUpd8.file());
        });

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
