
#include <QTest>
#include <QDebug>
#include <QTemporaryFile>

#include "autotest.h"
#include "cxxhash.h"


class CXXHashTest : public QObject {
    Q_OBJECT
private slots:
    void testDigestFile() {
        QTemporaryFile tmpFile;
        tmpFile.open();
        int fd = tmpFile.handle();

        std::string testStr  = "aa__bb__cc__dd";
        write(fd, testStr.c_str(), testStr.size());
        lseek(fd, 0, SEEK_SET);

        CXXHash h;
        CXXHash::DigestResult res{};

        res = h.digestFile(fd, 2, 4);
        QCOMPARE(res.countOfReads, 4);
        QCOMPARE(res.hash, XXH64("aabbccdd", 8, 0 ));

        // change maxCountOfReads
        lseek(fd, 0, SEEK_SET);
        res = h.digestFile(fd, 2, 4, 2);
        QCOMPARE(res.countOfReads, 2);
        QCOMPARE(res.hash, XXH64("aabb", 4, 0 ));

        // effectively digest everything
        lseek(fd, 0, SEEK_SET);
        res = h.digestFile(fd, 2, 2);
        QCOMPARE(res.countOfReads, 7);
        QCOMPARE(res.hash, XXH64(testStr.c_str(), 14, 0 ));

        // corner case: seekstep == bufsize + 1
        lseek(fd, 0, SEEK_SET);
        res = h.digestFile(fd, 2, 3);
        QCOMPARE(res.countOfReads, 5);
        QCOMPARE(res.hash, XXH64("aa_b__c_dd", 10, 0 ));

        // uneven character length should also work
        testStr  = "aa__bb__cc__d";
        ftruncate(fd, 0);
        lseek(fd, 0, SEEK_SET);
        write(fd, testStr.c_str(), testStr.size());
        lseek(fd, 0, SEEK_SET);
        res = h.digestFile(fd, 2, 4);
        QCOMPARE(res.countOfReads, 4);
        QCOMPARE(res.hash, XXH64("aabbccd", 7, 0 ));

    }


};


DECLARE_TEST(CXXHashTest)

#include "test_cxxhash.moc"
