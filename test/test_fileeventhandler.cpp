
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <QTest>
#include <QTemporaryFile>



#include "autotest.h"
#include "helper_for_test.h"

// #include "fdcontrol.h"
#include "util.h"
#include "os.h"
#include "osutil.h"

#include "settings.h"


#include "fileeventhandler.h"
#include "fileeventtypes.h"



void writeCompareBuf(const std::string & buf,
                     const std::string & hasStr,
                     const int fd, FileEventHandler& fEventHandler){
    ftruncate(fd, 0);
    write(fd, buf.c_str(), buf.size());
    lseek(fd, 0, SEEK_SET);
    struct stat stat_ = os::fstat(fd);
    DevInodePair devInodePair(stat_.st_dev, stat_.st_ino);

    fEventHandler.handleCloseWrite(fd);
    lseek(fd, 0, SEEK_SET);

    uint64_t correctHash = XXH64(hasStr.c_str(), hasStr.size(), 0 );
    QVERIFY(! fEventHandler.writeEvents().empty());
    auto eventInfo = fEventHandler.writeEvents().find(devInodePair).value();

    auto path = osutil::findPathOfFd<std::string>(fd);
    QCOMPARE(eventInfo.fullPath, path);
    QCOMPARE(correctHash, eventInfo.hash.value());
}

class FileEventHandlerTest : public QObject {
    Q_OBJECT
private slots:
    void init(){
        //testhelper::setupPaths();
    }

    void cleanup(){
        // testhelper::cleanupPaths();
    }

    void tWrite() {

        /// Primarily a test, if hashChunkSize and
        /// hashMaxCountOfReads are handled correctly
        QTemporaryFile tmpFile;
        QVERIFY(tmpFile.open());
        FileEventHandler fEventHandler;

        auto & sets = Settings::instance();
        sets.m_wSettings.includePaths.insert("/");

        sets.m_hashSettings.hashEnable = true;
        sets.m_hashSettings.hashMeta = HashMeta(2, 2);

        int fd = tmpFile.handle();

        std::string buf = "g";
        writeCompareBuf(buf, buf, fd, fEventHandler);

        buf = "gh";
        writeCompareBuf(buf, buf, fd, fEventHandler);

        buf = "abc";
        writeCompareBuf(buf, buf, fd, fEventHandler);

        buf = "abcd";
        writeCompareBuf(buf, buf, fd, fEventHandler);

        // only 2 chars (hashChunkSize) each at index 0 and
        // 10/hashMaxCountOfReads = 5 should be read and used for hash
        writeCompareBuf("ab___cd___", "abcd", fd, fEventHandler);

        sets.m_hashSettings.hashMeta = HashMeta(3, 2);
        writeCompareBuf("abc__def___", "abcdef", fd, fEventHandler);

        sets.m_hashSettings.hashMeta = HashMeta(1, 2);
        writeCompareBuf("a____d_____", "ad", fd, fEventHandler);

        sets.m_hashSettings.hashMeta = HashMeta(1, 3);
        writeCompareBuf("a__b__c___", "abc", fd, fEventHandler);

    }

    void tRead(){
        // TODO: implement a test...

        auto & readSettings = Settings::instance().m_scriptSettings;
        readSettings.enable = true;
        readSettings.includePaths.insert("/"); // todo: mk unique path
        readSettings.maxFileSize = 50000;
        readSettings.onlyWritable = true;
        readSettings.includeExtensions = {};
        readSettings.maxCountOfFiles = 1;
        readSettings.flushToDiskTotalSize = 10*1000;
    }

};



DECLARE_TEST(FileEventHandlerTest)

#include "test_fileeventhandler.moc"
