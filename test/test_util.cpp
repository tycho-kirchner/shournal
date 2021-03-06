



#include <QTest>
#include <QDebug>
#include <QTemporaryFile>

#include "autotest.h"


class UtilTest : public QObject {
    Q_OBJECT

    template<class T>
    void splitAbsPathTest(const T & p, const T& expectedPath, const T& expectedFile)
    {
        auto pair = splitAbsPath(p);
        QVERIFY(pair.first == expectedPath);
        QVERIFY(pair.second == expectedFile);
    }


private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }

    void testSplitAbsPath() {
        splitAbsPathTest<std::string>("/", "/", "");
        splitAbsPathTest<QString>("/", "/", "");

        splitAbsPathTest<std::string>("/home", "/", "home");
        splitAbsPathTest<QString>("/home", "/", "home");

        splitAbsPathTest<std::string>("/home/user", "/home", "user");
        splitAbsPathTest<QString>("/home/user", "/home", "user");

        splitAbsPathTest<std::string>("/home/user/foo", "/home/user", "foo");
        splitAbsPathTest<QString>("/home/user/foo", "/home/user", "foo");

    }

    void testPathJoinFilename(){
        QVERIFY(pathJoinFilename(QString("/"), QString("foo")) == "/foo");
        QVERIFY(pathJoinFilename(QString("/home/foo"), QString("bar")) == "/home/foo/bar");
    }


};


DECLARE_TEST(UtilTest)

#include "test_util.moc"
