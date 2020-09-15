
#include "qoutstream.h"
#include "qformattedstream.h"

#include "autotest.h"


class QFormattedtStreamTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }

    void testIt() {
        QString str;
        QFormattedStream s(&str);
        s.setLineStart("# ");
        s.setMaxLineWidth(5);

        s << "aha next\ntext\nFoo\nAVeryLongWord";
        s << "ok\n";
        s << "na";

        QCOMPARE(str, QString("# aha\n# nex\n# t\n# tex\n# t\n# Foo\n# AVe\n# ryL\n# ong\n"
                         "# Wor\n# d \n# ok\n# na "));
    }


};

DECLARE_TEST(QFormattedtStreamTest)

#include "test_qformattedstream.moc"
