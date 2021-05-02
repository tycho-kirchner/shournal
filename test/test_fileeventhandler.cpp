
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <QTest>
#include <QTemporaryFile>



#include "autotest.h"
#include "helper_for_test.h"

#include "util.h"
#include "os.h"
#include "osutil.h"
#include "settings.h"
#include "stdiocpp.h"

#include "fileeventhandler.h"


/// Write the content of buf to fd, let
/// the FileEventHandler process that file and
/// compare the partial hashes.
void writeCompareBuf(const std::string & buf,
                     const std::string & hasStr,
                     const int fd){
    ftruncate(fd, 0);
    write(fd, buf.c_str(), buf.size());
    lseek(fd, 0, SEEK_SET);

    FileEventHandler fEventHandler;
    fEventHandler.handleCloseWrite(fd);
    lseek(fd, 0, SEEK_SET);

    uint64_t correctHash = XXH64(hasStr.c_str(), hasStr.size(), 0 );

    stdiocpp::fseek(fEventHandler.fileEvents().file(), 0 , SEEK_SET);
    FileEvent* e = fEventHandler.fileEvents().read();
    QVERIFY(e != nullptr);

    auto path = osutil::findPathOfFd<std::string>(fd);
    //QIErr() << "std::string(e.fullPath), path" << QString(e.fullPath) << QString::fromStdString(path);
    QCOMPARE(std::string(e->path()), path);
    QVERIFY(! e->hash().isNull());
    QCOMPARE(correctHash, e->hash().value());
}

class FileEventHandlerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }

    void init(){
        //testhelper::setupPaths();
    }

    void cleanup(){
        // testhelper::cleanupPaths();
    }

    void tWrite() {
        /// Primarily a test, if hashChunkSize and
        /// hashMaxCountOfReads are handled correctly
        // Don't use QTemporaryFile here, we need a regular
        // file with st1.st_nlink > 0
        char tmpFileName[] = "fileevent_test_XXXXXX";
        int fd = mkstemp(tmpFileName);
        QVERIFY(os::fstat(fd).st_nlink > 0);
        auto rmTmpFile = finally([&tmpFileName] { remove(tmpFileName); });


        auto & sets = Settings::instance();
        sets.m_wSettings.includePaths->insert("/");

        sets.m_hashSettings.hashEnable = true;
        sets.m_hashSettings.hashMeta = HashMeta(2, 2);

        std::string buf = "g";
        writeCompareBuf(buf, buf, fd);

        buf = "gh";
        writeCompareBuf(buf, buf, fd);

        buf = "abc";
        writeCompareBuf(buf, buf, fd);

        buf = "abcd";
        writeCompareBuf(buf, buf, fd);

        // only 2 chars (hashChunkSize) each at index 0 and
        // 10/hashMaxCountOfReads = 5 should be read and used for hash
        writeCompareBuf("ab___cd___", "abcd", fd);

        sets.m_hashSettings.hashMeta = HashMeta(3, 2);
        writeCompareBuf("abc__def___", "abcdef", fd);

        sets.m_hashSettings.hashMeta = HashMeta(1, 2);
        writeCompareBuf("a____d_____", "ad", fd);

        sets.m_hashSettings.hashMeta = HashMeta(1, 3);
        writeCompareBuf("a__b__c___", "abc", fd);

    }

    void tRead(){
        // TODO: implement a test...

        auto & readSettings = Settings::instance().m_scriptSettings;
        readSettings.enable = true;
        readSettings.includePaths->insert("/"); // todo: mk unique path
        readSettings.maxFileSize = 50000;
        readSettings.onlyWritable = true;
        readSettings.includeExtensions = {};
        readSettings.maxCountOfFiles = 1;
        readSettings.flushToDiskTotalSize = 10*1000;
    }

};



DECLARE_TEST(FileEventHandlerTest)

#include "test_fileeventhandler.moc"

