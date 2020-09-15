
#include <QTest>
#include <QTemporaryFile>
#include <cassert>

#include "autotest.h"
#include "helper_for_test.h"
#include "util.h"
#include "database/fileinfos.h"

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

    FileReadEvent e{};

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

void push_back_writeEvent(FileWriteEvents& fwriteEvents, const FileWriteEvent& e){
    struct stat st{};
    st.st_mtime = e.mtime;
    st.st_size = e.size;
    fwriteEvents.write(e.fullPath, st, e.hash);
}

void push_back_readEvent(FileReadEvents& freadEvents, const FileReadEventForTest_ptr& e){
    struct stat st{};
    st.st_mtime = e->e.mtime;
    st.st_size = e->e.size;
    st.st_mode = e->e.mode;
    freadEvents.write(e->e.fullPath, st, e->e.hash, e->file().handle(), true );
}

FileWriteEvent generateFileWriteEvent(){
    static auto hash_ = std::numeric_limits<uint64_t>::max();
    static int id_ = 1;

    FileWriteEvent e{};
    e.hash = hash_;

    std::string fullpath = "/tmp/" + std::to_string(id_) +  ".txt";
    strcpy(e.fullPath, fullpath.c_str());
    e.size = id_;
    e.mtime = QDateTime(QDate(2019,1, id_ % 28)).toTime_t();

    hash_--;
    id_++;

    return  e;
}


FileReadEventForTest_ptr
generateFileReadEvent(){
    static auto hash_ = std::numeric_limits<uint64_t>::max();
    static int id_ = 1;
    auto e = FileReadEventForTest_ptr(new FileReadEventForTest(QByteArray::number(id_)));
    e->e.mode = S_IREAD;
    e->e.size = id_;
    e->e.mtime = QDateTime(QDate(2019,1, id_ % 28)).toTime_t();
    std::string fullpath = "/tmp/" + std::to_string(id_) +  ".txt";
    strcpy(e->e.fullPath, fullpath.c_str());
    e->e.hash = hash_;

    --hash_;
    ++id_;
    return e;
}

FileWriteInfo fileWriteEventToWriteInfo(const FileWriteEvent& e){
    FileWriteInfo i;
    assert(! e.hashIsNull);
    i.hash = e.hash;

    auto splittedPah = splitAbsPath(QString(e.fullPath));
    i.path = splittedPah.first;
    i.name = splittedPah.second;
    i.size = e.size;
    i.mtime = db_conversions::fromMtime(e.mtime).toDateTime();
    return i;
}



FileReadInfo fileReadEventToReadInfo(const FileReadEventForTest_ptr& e){
    FileReadInfo i;
    i.mode = e->e.mode;

    auto splittedPah = splitAbsPath(QString(e->e.fullPath));
    i.path = splittedPah.first;
    i.name = splittedPah.second;
    i.size = e->e.size;
    i.mtime = db_conversions::fromMtime(e->e.mtime).toDateTime();
    i.hash = e->e.hash;
    return i;
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

void db_addFileEventsWrapper(const CommandInfo &cmd, FileWriteEvents &writeEvents,
                             FileReadEvents &readEvents){
    writeEvents.fseekToBegin();
    readEvents.fseekToBegin();
    db_controller::addFileEvents(cmd, writeEvents, readEvents);
}

class DbCtrlTest : public QObject {
    Q_OBJECT
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
        QTemporaryDir tmpDir;
        FileWriteEvents writeEvents(tmpDir.path().toUtf8());
        FileReadEvents readEvents(tmpDir.path().toUtf8());

        auto fInfo1 = generateFileWriteEvent();
        push_back_writeEvent(writeEvents, fInfo1);

        auto fInfo2 = generateFileWriteEvent();
        push_back_writeEvent(writeEvents, fInfo2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });

        db_addFileEventsWrapper(cmd1, writeEvents,
                                     readEvents);

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.wFile_size, int(fInfo1.size) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(fInfo1), fileWriteEventToWriteInfo(fInfo2) };
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);
        q1.clear();
        q1.addWithAnd(queryCols.wFile_hash, qBytesFromVar(fInfo1.hash) );
        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        // TODO: test with a hash of null

    }

    void tRead(){
        QTemporaryDir tmpDir;
        FileWriteEvents fwriteEvents(tmpDir.path().toUtf8());
        FileReadEvents readEvents(tmpDir.path().toUtf8());
        auto readEvent1 = generateFileReadEvent();
        push_back_readEvent(readEvents, readEvent1);
        auto readEvent2 = generateFileReadEvent();
        push_back_readEvent(readEvents, readEvent2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });

        db_addFileEventsWrapper(cmd1, fwriteEvents, readEvents );

        cmd1.fileReadInfos = {fileReadEventToReadInfo(readEvent1), fileReadEventToReadInfo(readEvent2)};

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.rFile_size, int(readEvent1->e.size) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        q1.clear();

        q1.addWithAnd(queryCols.rFile_size, int(readEvent2->e.size) );

        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        // TODO: also check bytes of the file?!
    }


    void tDeleteCommand(){
        QTemporaryDir tmpDir;
        FileWriteEvents fwriteEvents(tmpDir.path().toUtf8());
        FileReadEvents freadEvents(tmpDir.path().toUtf8());

        auto readEvent1 = generateFileReadEvent();
        push_back_readEvent(freadEvents, readEvent1);
        auto readEvent2 = generateFileReadEvent();
        push_back_readEvent(freadEvents, readEvent2);

        auto writeEvent1 = generateFileWriteEvent();
        push_back_writeEvent(fwriteEvents, writeEvent1);

        auto writeEvent2 = generateFileWriteEvent();
        push_back_writeEvent(fwriteEvents, writeEvent2);

        CommandInfo cmd1 = generateCmdInfo();
        cmd1.idInDb = db_controller::addCommand(cmd1);
        auto closeDb = finally([] { db_connection::close(); });
        db_addFileEventsWrapper(cmd1, fwriteEvents, freadEvents );

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

        freadEvents.clear();
        fwriteEvents.clear();

        push_back_readEvent(freadEvents, readEvent1);
        push_back_readEvent(freadEvents, readEvent2);

        db_addFileEventsWrapper(cmd1, fwriteEvents, freadEvents );

        cmd2.idInDb = db_controller::addCommand(cmd2);
        freadEvents.clear();

        push_back_readEvent(freadEvents, readEvent1);
        push_back_readEvent(freadEvents, readEvent3);

        db_addFileEventsWrapper(cmd2, fwriteEvents, freadEvents );

        QCOMPARE(deleteCommandInDb(cmd1.idInDb), 1);
        // readEvent1 is common to both and should remain, readEvent2 should be deleted,
        // readEvent3 should still be there
        const char* qReadFileSize = "select * from readFile where size=?";

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent1->e.size));
        query->exec();
        QVERIFY(query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent2->e.size));
        query->exec();
        QVERIFY(! query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent3->e.size));
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

        QCOMPARE(countStoredFiles(), 0);
    }


};


DECLARE_TEST(DbCtrlTest)

#include "test_db_controller.moc"

