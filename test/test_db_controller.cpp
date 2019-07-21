
#include <QTest>


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


using db_controller::QueryColumns;
using db_controller::queryForCmd;


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

FileWriteEvent generateFileWriteEvent(){
    static auto hash_ = std::numeric_limits<uint64_t>::max();
    static int id_ = 1;

    FileWriteEvent e;
    e.hash = hash_;
    e.fullPath = "/tmp/" + std::to_string(id_) + ".txt";
    e.size = id_;
    e.mtime = QDateTime(QDate(2019,1, id_ % 28)).toTime_t();

    hash_--;
    id_++;

    return  e;
}

FileReadEvent generateFileReadEvent(){
    static int id_ = 1;
    FileReadEvent e;
    e.mode = S_IREAD;
    e.size = id_;
    e.bytes = QByteArray::number(id_);
    e.mtime = QDateTime(QDate(2019,1, id_ % 28)).toTime_t();
    e.fullPath = "/tmp/" + std::to_string(id_) + ".txt";

    id_++;
    return e;
}

FileWriteInfo fileWriteEventToWriteInfo(const FileWriteEvent& e){
    FileWriteInfo i;
    i.hash = e.hash;
    auto splittedPah = splitAbsPath(e.fullPath);
    i.path = QString::fromStdString(splittedPah.first);
    i.name = QString::fromStdString(splittedPah.second);
    i.size = e.size;
    i.mtime = db_conversions::fromMtime(e.mtime).toDateTime();
    return i;
}



FileReadInfo fileReadEventToReadInfo(const FileReadEvent& e){
    FileReadInfo i;
    i.mode = e.mode;

    auto splittedPah = splitAbsPath(e.fullPath);
    i.path = QString::fromStdString(splittedPah.first);
    i.name = QString::fromStdString(splittedPah.second);
    i.size = e.size;
    i.mtime = db_conversions::fromMtime(e.mtime).toDateTime();
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


class DbCtrlTest : public QObject {
    Q_OBJECT
private slots:
    void init(){
        testhelper::setupPaths();
    }

    void cleanup(){
        testhelper::deletePaths();
    }


    void tWriteOnly() {
        CommandInfo cmd1 = generateCmdInfo();

        FileWriteEventHash fInfos;
        ulong fCounter = 1;
        auto fInfo1 = generateFileWriteEvent();
        fInfos.insert({fCounter,fCounter}, fInfo1);
        ++fCounter;

        auto fInfo2 = generateFileWriteEvent();
        fInfos.insert({fCounter,fCounter}, fInfo2);
        ++fCounter;

        auto cmdId = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });
        db_controller::addFileEvents(cmdId, fInfos, FileReadEventHash());

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.wFile_size, int(fInfo1.size) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(fInfo1), fileWriteEventToWriteInfo(fInfo2) };
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);
        q1.clear();
        q1.addWithAnd(queryCols.wFile_hash, qBytesFromVar(fInfo1.hash.value()) );
        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileWriteInfos(cmd1Back->value().fileWriteInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        // TODO: test with a hash of null

    }

    void tRead(){
        CommandInfo cmd1 = generateCmdInfo();
        ulong fCounter = 1;
        FileReadEventHash readEvents;
        auto readEvent1 = generateFileReadEvent();
        readEvents.insert({fCounter, fCounter}, readEvent1);
        fCounter++;
        auto readEvent2 = generateFileReadEvent();
        readEvents.insert({fCounter, fCounter}, readEvent2);
        fCounter++;

        auto cmd1Id = db_controller::addCommand(cmd1);
        auto closeDb = finally([] {
            db_connection::close();
        });

        db_controller::addFileEvents(cmd1Id, FileWriteEventHash(), readEvents );

        cmd1.fileReadInfos = {fileReadEventToReadInfo(readEvent1), fileReadEventToReadInfo(readEvent2)};

        QueryColumns & queryCols = QueryColumns::instance();
        SqlQuery q1;
        q1.addWithAnd(queryCols.rFile_size, int(readEvent1.size) );

        auto cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);

        q1.clear();

        q1.addWithAnd(queryCols.rFile_size, int(readEvent2.size) );

        cmd1Back = queryForCmd(q1);
        QVERIFY(cmd1Back->next());
        sortFileReadInfos(cmd1Back->value().fileReadInfos);
        QCOMPARE(cmd1Back->value(), cmd1);
    }


    void tDeleteCommand(){
        ulong fCounter = 1;
        FileReadEventHash readEvents;
        auto readEvent1 = generateFileReadEvent();
        readEvents.insert({fCounter, fCounter}, readEvent1);
        fCounter++;
        auto readEvent2 = generateFileReadEvent();
        readEvents.insert({fCounter, fCounter}, readEvent2);
        fCounter++;

        FileWriteEventHash writeEvents;
        auto writeEvent1 = generateFileWriteEvent();
        writeEvents.insert({fCounter,fCounter}, writeEvent1);
        ++fCounter;

        auto writeEvent2 = generateFileWriteEvent();
        writeEvents.insert({fCounter,fCounter}, writeEvent2);
        ++fCounter;

        CommandInfo cmd1 = generateCmdInfo();
        auto cmd1Id = db_controller::addCommand(cmd1);
        auto closeDb = finally([] { db_connection::close(); });
        db_controller::addFileEvents(cmd1Id, writeEvents, readEvents );

        cmd1.fileReadInfos = {fileReadEventToReadInfo(readEvent1),fileReadEventToReadInfo(readEvent2)};
        cmd1.fileWriteInfos = { fileWriteEventToWriteInfo(writeEvent1), fileWriteEventToWriteInfo(writeEvent2) };

        QCOMPARE(deleteCommandInDb(cmd1Id), 1);

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

        cmd1Id = db_controller::addCommand(cmd1);
        readEvents.clear();
        readEvents.insert({fCounter, fCounter}, readEvent1);
        fCounter++;
        readEvents.insert({fCounter, fCounter}, readEvent2);
        fCounter++;
        db_controller::addFileEvents(cmd1Id, FileWriteEventHash(), readEvents );

        auto cmd2Id = db_controller::addCommand(cmd2);
        readEvents.clear();
        readEvents.insert({fCounter, fCounter}, readEvent1);
        fCounter++;
        readEvents.insert({fCounter, fCounter}, readEvent3);
        fCounter++;
        db_controller::addFileEvents(cmd2Id, FileWriteEventHash(), readEvents );

        QCOMPARE(deleteCommandInDb(cmd1Id), 1);
        // readEvent1 is common to both and should remain, readEvent2 should be deleted,
        // readEvent3 should still be there
        const char* qReadFileSize = "select * from readFile where size=?";

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent1.size));
        query->exec();
        QVERIFY(query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent2.size));
        query->exec();
        QVERIFY(! query->next());

        query->prepare(qReadFileSize);
        query->addBindValue(qint64(readEvent3.size));
        query->exec();
        QVERIFY(query->next());

        QCOMPARE(countStoredFiles(), 2);

        QCOMPARE(deleteCommandInDb(cmd2Id), 1);
        query->exec("select * from writtenFile");
        QVERIFY(! query->next());

        query->exec("select * from readFile");
        QVERIFY(! query->next());

        query->exec("select * from readFileCmd");
        QVERIFY(! query->next());

        QCOMPARE(countStoredFiles(), 0);

    }

};


DECLARE_TEST(DbCtrlTest)

#include "test_db_controller.moc"
