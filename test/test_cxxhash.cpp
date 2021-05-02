
#include <QTest>
#include <QDebug>
#include <QTemporaryFile>

#include "autotest.h"
#include "cxxhash.h"


class CXXHashTest : public QObject {
    Q_OBJECT


    bool test_hash(int fd, const std::string& str, int chunksize,
                   int seekstep, int maxCountOfReads, int bufsize,
                   CXXHash& h, std::string expected=""){
        ftruncate(fd, 0);
        lseek(fd, 0, SEEK_SET);
        os::write(fd, str);
        lseek(fd, 0, SEEK_SET);

        h.resizeBuf(bufsize);
        struct partial_xxhash_result res = h.digestFile(fd, chunksize, seekstep, maxCountOfReads);

        if(expected.empty()){
            // delete all underscores (jumped over during partial hash)
            expected = str;
            expected.erase(std::remove(expected.begin(), expected.end(), '_'), expected.end());
        }
        return res.hash == XXH64(expected.c_str(), expected.size(), 0 );
    }


private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }


    void testDigestFile() {
        QTemporaryFile tmpFile;
        tmpFile.open();
        int fd = tmpFile.handle();
        CXXHash h;
        const int ignoreMaxReads = std::numeric_limits<int>::max();

       // result should be independent from buffer size
       for(int bufSize=1; bufSize < 128; bufSize++){
           QVERIFY(test_hash(fd, "aa__bb__cc__dd", 2, 4, ignoreMaxReads, bufSize, h));
           // change maxCountOfReads
           QVERIFY(test_hash(fd, "aa__bb__cc__dd", 2, 4, 2, bufSize, h, "aabb"));
           // effectively digest everything
           QVERIFY(test_hash(fd, "aa__bb__cc__dd", 2, 2, ignoreMaxReads, bufSize, h,
                             "aa__bb__cc__dd"));
           // corner case: seekstep == chunksize + 1
           QVERIFY(test_hash(fd, "aa__bb__cc__dd", 2, 3, ignoreMaxReads, bufSize, h,
                             "aa_b__c_dd"));
           // uneven character length should also work
           QVERIFY(test_hash(fd, "aa__bb__cc__d", 2, 4, ignoreMaxReads, bufSize, h));
           // larger chunks
           QVERIFY(test_hash(fd, "aaaa__bbbb__cccc__dddd", 4, 6, ignoreMaxReads, bufSize, h));
       }
    }



};


DECLARE_TEST(CXXHashTest)

#include "test_cxxhash.moc"
