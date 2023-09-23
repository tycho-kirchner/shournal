
#include <QTest>
#include <QTemporaryFile>
#include <cassert>
#include <fcntl.h>

#include "autotest.h"
#include "osutil.h"
#include "helper_for_test.h"
#include "util.h"
#include "database/fileinfos.h"
#include "fileevents.h"

#include "database/db_controller.h"
#include "database/db_connection.h"
#include "database/query_columns.h"
#include "database/db_conversions.h"
#include "database/storedfiles.h"
#include "cleanupresource.h"
#include "settings.h"
#include "qfilethrow.h"
#include "stdiocpp.h"




using db_controller::QueryColumns;
using db_controller::queryForCmd;

template <class ContainerT>
typename ContainerT::value_type* __fInfoById(ContainerT& infos, qint64 id_){
    for(auto& f : infos){
        if(f.idInDb == id_) return &f;
    }
    return nullptr;
}


/// Stored read files may be moved
/// from cache dir to shournal's db,
/// to simplify comparison here, store
/// the content in 'bytes'.
class FileReadEventForTest {
public:
    FileReadEventForTest(const QByteArray& bytes) :
        m_bytes(bytes)
    {
        m_file.open();
        m_file.write(bytes);
        m_file.seek(0);
    }

    FileEvent e{};

    const QTemporaryFile& file(){ return m_file; }
    const QByteArray& bytes(){ return m_bytes; }

private:
    QTemporaryFile m_file;
    QByteArray m_bytes;
};

typedef std::unique_ptr<FileReadEventForTest> FileReadEventForTest_ptr;


CommandInfo generateCmdInfo(){
    static int id_ = 1;
    CommandInfo cmd;
    cmd.text = QByteArray::number(id_);
    cmd.hashMeta.chunkSize = 2048;
    cmd.hashMeta.maxCountOfReads = 20;
    cmd.hostname = "myhost";
    cmd.username = "myuser";
    cmd.returnVal = 42;
    cmd.startTime = QDateTime(QDate(2019,1, id_ % 28));
    cmd.endTime = QDateTime(QDate(2019,1, id_ % 28));
    cmd.workingDirectory = "/home/user";

    id_++;
    return cmd;
}

void push_back_writeEvent(FileEvents& fEvents, const FileEvent& e){
    struct stat st{};
    st.st_mtime = e.mtime();
    st.st_size = e.size();
    st.st_mode = mode_t(e.mode());
    fEvents.write(e.flags(), e.path(), st, e.hash());
}

void push_back_readEvent(FileEvents& fEvents, const FileReadEventForTest_ptr& e){
    struct stat st = os::fstat(e->file().handle());
    fEvents.write(e->e.flags(), e->e.path(), st, e->e.hash(), e->file().handle());
}


// sort them by filesize
void sortFileWriteInfos(FileWriteInfos & fInos){
    std::sort(fInos.begin(), fInos.end(), [](const FileWriteInfo& f1, const FileWriteInfo& f2){
        return f1.size < f2.size;
    });
}

// sort them by filesize
void sortFileReadInfos(FileReadInfos & fInos){
    std::sort(fInos.begin(), fInos.end(), [](const FileReadInfo& f1, const FileReadInfo& f2){
        return f1.size < f2.size;
    });
}

int countStoredFiles(){
    return QDir(StoredFiles::getReadFilesDir()).entryList(QDir::Filter::NoDotDot | QDir::Files).size();
}

int deleteCommandInDb(qint64 id)
{
   SqlQuery q;
   q.addWithAnd(QueryColumns::instance().cmd_id, id, E_CompareOperator::EQ);
   return db_controller::deleteCommand(q);
}

void db_addFileEventsWrapper(const CommandInfo &cmd, FileEvents &fileEvents){
    fseek(fileEvents.file(), 0, SEEK_SET);
    db_controller::addFileEvents(cmd, fileEvents);
}

class DbCtrlTest : public QObject {
    Q_OBJECT

    FileWriteInfo fileWriteEventToWriteInfo(const FileEvent& e){
        FileWriteInfo i;
        assert(! e.m_close_event.hash_is_null);
        i.hash = e.hash();

        auto splittedPah = splitAbsPath(QString(e.path()));
        i.path = splittedPah.first;
        i.name = splittedPah.second;
        i.size = e.size();
        i.mtime = db_conversions::fromMtime(e.mtime()).toDateTime();
        return i;
    }

    FileReadInfo fileReadEventToReadInfo(const FileReadEventForTest_ptr& e){
        FileReadInfo i;
        i.mode = mode_t(e->e.mode());

        auto splittedPah = splitAbsPath(QString(e->e.path()));
        i.path = splittedPah.first;
        i.name = splittedPah.second;
        i.size = e->e.size();
        i.mtime = db_conversions::fromMtime(e->e.mtime()).toDateTime();
        i.hash = e->e.hash();
        return i;
    }



    FileEvent generateFileWriteEvent(){
        static auto hash_ = std::numeric_limits<uint64_t>::max();
        static int id_ = 1;

        FileEvent e{};
        e.m_close_event.flags = O_WRONLY;
        e.m_close_event.mtime = QDateTime(QDate(2019,1, id_ % 28)).toTime_t();
        e.m_close_event.size = id_;
        e.m_close_event.mode = 0;
        e.m_close_event.hash = hash_;
        e.m_close_event.hash_is_null = false;
        e.m_close_event.bytes = 0;

        std::string fullpath = "/tmp/" + std::to_string(id_) +  ".txt";
        e.setPath(fullpath.c_str());
        hash_--;
        id_++;

        return  e;
    }


    FileReadEventForTest_ptr
    generateFileReadEvent(){
        static auto hash_ = std::numeric_limits<uint64_t>::max();
        static int id_ = 1;
        QByteArray fileContent(QByteArray::number(id_), id_);
        auto e = FileReadEventForTest_ptr(new FileReadEventForTest(fileContent));
        auto st = os::fstat(e->file().handle());

        e->e.m_close_event.flags = O_RDONLY;
        e->e.m_close_event.mtime = st.st_mtime;
        e->e.m_close_event.size = st.st_size;
        e->e.m_close_event.mode = st.st_mode;
        e->e.m_close_event.hash = hash_;
        e->e.m_close_event.hash_is_null = false;
        e->e.m_close_event.bytes = st.st_size;

        std::string fullpath = "/tmp/" + std::to_string(id_) +  ".txt";
        e->e.setPath(fullpath.c_str());

        --hash_;
        ++id_;
        return e;
    }


private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }

    void init(){
        testhelper::setupPaths();
    }

    void cleanup(){
        testhelper::deletePaths();
    }


    void tWriteOnly() {
        FILE* tmpFile = stdiocpp::tmpfile();
        auto closeTmpFile = finally([&tmpFile] {
            fclose(tmpFile);
        });
        FileEvents fileEvents;
        fileEvents.setFile(tmpFile);


        auto fInfo1 = generateFileWriteEvent();
        push_back_writeEvent(fileEvents, fInfo1);

        auto fInfo2 = generateFileWriteEvent();
        push_back_writeEvent(fileEvents, fInfo2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });

        db_addFileEventsWrapper(cmd1, fileEvents);

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.wFile_size, int(fInfo1.size()) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(fInfo1),
                                fileWriteEventToWriteInfo(fInfo2) };
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);
        q1.clear();
        q1.addWithAnd(queryCols.wFile_hash, qBytesFromVar(fInfo1.hash().value()) );
        cmd1Back.reset();
        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        // TODO: test with a hash of null

    }


    void tRead(){
        FILE* tmpFile = stdiocpp::tmpfile();
        auto closeTmpFile = finally([&tmpFile] {
            fclose(tmpFile);
        });
        FileEvents fileEvents;
        fileEvents.setFile(tmpFile);

        auto readEvent1 = generateFileReadEvent();
        push_back_readEvent(fileEvents, readEvent1);
        auto readEvent2 = generateFileReadEvent();
        push_back_readEvent(fileEvents, readEvent2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });
        fflush(tmpFile);
        db_addFileEventsWrapper(cmd1, fileEvents);

        cmd1.fileReadInfos = {fileReadEventToReadInfo(readEvent1), fileReadEventToReadInfo(readEvent2)};

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.rFile_size, int(readEvent1->e.size()) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        q1.clear();

        q1.addWithAnd(queryCols.rFile_size, int(readEvent2->e.size()) );
        cmd1Back.reset();
        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        // TODO: also check bytes of the file?!
    }

    void tDuplicates(){
        FILE* tmpFile = stdiocpp::tmpfile();
        auto closeTmpFile = finally([&tmpFile] {
            fclose(tmpFile);
        });
        FileEvents fileEvents;
        fileEvents.setFile(tmpFile);


        auto wInfo1 = generateFileWriteEvent();
        push_back_writeEvent(fileEvents, wInfo1);
        push_back_writeEvent(fileEvents, wInfo1);
        push_back_writeEvent(fileEvents, wInfo1);

        auto rInfo1 = generateFileReadEvent();
        push_back_readEvent(fileEvents, rInfo1);
        push_back_readEvent(fileEvents, rInfo1);
        push_back_readEvent(fileEvents, rInfo1);


        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });

        db_addFileEventsWrapper(cmd1, fileEvents);


        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.cmd_id, int(cmd1.idInDb) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(wInfo1)};
        cmd1.fileReadInfos = { fileReadEventToReadInfo(rInfo1)};

        QCOMPARE(cmd1Back->value(), cmd1);
    }


    void tDeleteCommand(){
        FILE* tmpFile = stdiocpp::tmpfile();
        auto closeTmpFile = finally([&tmpFile] {
            fclose(tmpFile);
        });
        FileEvents fileEvents;
        fileEvents.setFile(tmpFile);

        auto readEvent1 = generateFileReadEvent();
        push_back_readEvent(fileEvents, readEvent1);
        auto readEvent2 = generateFileReadEvent();
        push_back_readEvent(fileEvents, readEvent2);

        auto writeEvent1 = generateFileWriteEvent();
        push_back_writeEvent(fileEvents, writeEvent1);

        auto writeEvent2 = generateFileWriteEvent();
        push_back_writeEvent(fileEvents, writeEvent2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] { db_connection::close(); });
        db_addFileEventsWrapper(cmd1, fileEvents );

        cmd1.fileReadInfos = {fileReadEventToReadInfo(readEvent1),
                              fileReadEventToReadInfo(readEvent2)};
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(writeEvent1),
                                fileWriteEventToWriteInfo(writeEvent2) };

        QCOMPARE(deleteCommandInDb(cmd1.idInDb), 1);

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        // should return all commands
        q1.addWithAnd(queryCols.rFile_size, 0, E_CompareOperator::GE );

        auto cmd1Back = queryForCmd(q1);

        QVERIFY(! cmd1Back->next());

        auto query = db_connection::mkQuery();
        query->exec("select * from writtenFile");
        QVERIFY(! query->next());

        query->exec("select * from readFile");
        QVERIFY(! query->next());

        query->exec("select * from readFileCmd");
        QVERIFY(! query->next());

        QCOMPARE(countStoredFiles(), 0);

        // a single command seems to work
        // Check for two commands, where one read file is unique for each command
        // while the other is common to both.
        auto cmd2 = generateCmdInfo();
        auto readEvent3 = generateFileReadEvent();
        cmd2.fileReadInfos = {fileReadEventToReadInfo(readEvent1),fileReadEventToReadInfo(readEvent3)};

        cmd1.idInDb = db_controller::addCommand(cmd1);

        stdiocpp::ftruncate_unlocked(fileEvents.file());

        push_back_readEvent(fileEvents, readEvent1);
        push_back_readEvent(fileEvents, readEvent2);

        db_addFileEventsWrapper(cmd1, fileEvents );

        cmd2.idInDb = db_controller::addCommand(cmd2);

        stdiocpp::ftruncate_unlocked(fileEvents.file());

        push_back_readEvent(fileEvents, readEvent1);
        push_back_readEvent(fileEvents, readEvent3);

        db_addFileEventsWrapper(cmd2, fileEvents );

        QCOMPARE(deleteCommandInDb(cmd1.idInDb), 1);
        // readEvent1 is common to both and should remain, readEvent2 should be deleted,
        // readEvent3 should still be there
        const char* qReadFileSize = "select * from readFile where size=?";

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent1->e.size()));
        query->exec();
        QVERIFY(query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent2->e.size()));
        query->exec();
        QVERIFY(! query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent3->e.size()));
        query->exec();
        QVERIFY(query->next());

        QCOMPARE(countStoredFiles(), 2);

        QCOMPARE(deleteCommandInDb(cmd2.idInDb), 1);
        query->exec("select * from writtenFile");
        QVERIFY(! query->next());

        query->exec("select * from readFile");
        QVERIFY(! query->next());

        query->exec("select * from readFileCmd");
        QVERIFY(! query->next());

        query->exec("select * from hashmeta");
        QVERIFY(! query->next());

        query->exec("select * from pathtable");
        QVERIFY(! query->next());

        QCOMPARE(countStoredFiles(), 0);
    }

    void tSchemeUpdates(){
        const QString & dbDir = db_connection::getDatabaseDir();
        os::rmdir(dbDir.toUtf8());

        // Copy a database with sample data to the test-database-dir
        // and check, whether the data survives the scheme update(s), which
        // are automatically performed upon the first database-usage.
        // Until including v2.2 nothing testworthy happened
        // -> src-path for database defined in cmake.
        QString testDbPath;
        if (QDir(SHOURNALTEST_SQLITE_v_2_2).exists()){
            testDbPath = SHOURNALTEST_SQLITE_v_2_2;
        } else {
            // also consider current directory to allow for easy testing
            // on another machine.
            testDbPath = splitAbsPath<QString>(SHOURNALTEST_SQLITE_v_2_2).second;
            if (! QDir(testDbPath).exists()){
                QIErr() << QString("dir of testdatabase not found: %1").arg(testDbPath);
                QVERIFY(false);
            }
        }
        QVERIFY(testhelper::copyRecursively(testDbPath, dbDir));

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q;
        q.addWithAnd(queryCols.wFile_id, 1);

        auto cmd = queryForCmd(q);
        QCOMPARE(cmd->computeSize(), 1);
        QVERIFY(cmd->next());
        QCOMPARE(cmd->value().fileWriteInfos.size(),2);
        auto fw = __fInfoById(cmd->value().fileWriteInfos, 1);
        QVERIFY(fw);
        QVERIFY(fw->name == "one");
        QVERIFY(fw->path == "/home/tycho");

        fw = __fInfoById(cmd->value().fileWriteInfos, 2);
        QVERIFY(fw);
        QVERIFY(fw->name == "two");
        QVERIFY(fw->path == "/home/tycho");


        // ---------
        q.clear();
        q.addWithAnd(queryCols.cmd_id, 2);
        cmd.reset();
        cmd = queryForCmd(q);
        QVERIFY(cmd->next());
        QCOMPARE(cmd->value().fileReadInfos.size(),2);
        auto fr = __fInfoById(cmd->value().fileReadInfos, 1);
        QVERIFY(fr);
        QVERIFY(fr->name == "one");
        QVERIFY(fr->path == "/home/tycho");
        QVERIFY(!fr->isStoredToDisk);

        fr = __fInfoById(cmd->value().fileReadInfos, 2);
        QVERIFY(fr);
        QVERIFY(fr->name == "two");
        QVERIFY(fr->path == "/home/tycho");
        QVERIFY(!fr->isStoredToDisk);

        // ---------

        q.clear();
        q.addWithAnd(queryCols.cmd_id, 3);
        cmd.reset();
        cmd = queryForCmd(q);
        QVERIFY(cmd->next());
        QCOMPARE(cmd->value().fileWriteInfos.size(),2);
        fw = __fInfoById(cmd->value().fileWriteInfos, 3);
        QVERIFY(fw);
        QVERIFY(fw->name == "three");
        QVERIFY(fw->path == "/tmp");

        fw = __fInfoById(cmd->value().fileWriteInfos, 4);
        QVERIFY(fw);
        QVERIFY(fw->name == "four");
        QVERIFY(fw->path == "/tmp");

        // ---------
        q.clear();
        q.addWithAnd(queryCols.cmd_id, 4);
        cmd.reset();
        cmd = queryForCmd(q);
        QVERIFY(cmd->next());
        QCOMPARE(cmd->value().fileReadInfos.size(),2);
        fr = __fInfoById(cmd->value().fileReadInfos, 3);
        QVERIFY(fr);
        QVERIFY(fr->name == "script1.sh");
        QVERIFY(fr->path == "/home/tycho/storeme");
        QVERIFY(fr->isStoredToDisk);

        fr = __fInfoById(cmd->value().fileReadInfos, 4);
        QVERIFY(fr);
        QVERIFY(fr->name == "script2.sh");
        QVERIFY(fr->path == "/home/tycho/storeme");
        QVERIFY(fr->isStoredToDisk);

    }


};


DECLARE_TEST(DbCtrlTest)

#include "test_db_controller.moc"

