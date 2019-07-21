
#include <QTest>
#include <QDebug>
#include <QTemporaryFile>

#include "autotest.h"

#include "osutil.h"

using namespace osutil;

class OsutilTest : public QObject {
    Q_OBJECT
private slots:
    void testReadWholeFile() {
        QTemporaryFile f;
        QVERIFY(f.open());
        QByteArray val("123456");
        f.write(val);
        f.seek(0);
        // different buffer sizes should ot change the result
        QCOMPARE(readWholeFile(f.handle(), 6), val);
        f.seek(0);
        QCOMPARE(readWholeFile(f.handle(), 7), val);
        f.seek(0);
        QCOMPARE(readWholeFile(f.handle(), 3), val);
        f.seek(0);
        QCOMPARE(readWholeFile(f.handle(), 1), val);
        f.seek(0);

    }


};


DECLARE_TEST(OsutilTest)

#include "test_osutil.moc"
