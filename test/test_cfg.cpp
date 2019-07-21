

#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>

#include "cfg.h"

#include "autotest.h"

using qsimplecfg::Cfg;
using qsimplecfg::Section;


class CfgTest : public QObject {
    Q_OBJECT

    const QString cfg = R"SOMERANDOMTEXT(
# Initial
# comment

[section1]
# section1
# comment
section1_key1=val1
section1_key2 =  val2

[section2]
# section2
# comment
section2_key1 = '''first
    second
    third'''
section2_key2='''
one
    two

three
'''

[section3]
section3_key1 = '''
'''
section3_key2 = '''
foo
'''

)SOMERANDOMTEXT";

    void verifyCfg(Cfg& cfg){
        QVERIFY(cfg.sectionHash().find("section1") != cfg.sectionHash().end());
        auto & sect1 = cfg["section1"];
        QCOMPARE(sect1.getValue<QString>("section1_key1"), QString("val1") );
        QCOMPARE(sect1.getValue<QString>("section1_key2"), QString("val2") );

        QVERIFY(cfg.sectionHash().find("section2") != cfg.sectionHash().end());
        auto & sect2 = cfg["section2"];
        QCOMPARE(sect2.getValues<QStringList>("section2_key1", {},false, false, "\n"),
                                            (QStringList{"first", "second", "third"}) );
        QCOMPARE(sect2.getValues<QStringList>("section2_key2", {},false, false, "\n"),
                 (QStringList{"one", "two", "three"}) );

        QVERIFY(cfg.sectionHash().find("section3") != cfg.sectionHash().end());
        auto & sect3 = cfg["section3"];
        QCOMPARE(sect3.getValues<QStringList>("section3_key1", {},false, false, "\n"), QStringList());
        QCOMPARE(sect3.getValues<QStringList>("section3_key2", {},false, false, "\n"),
                 (QStringList{"foo"}) );

        QVERIFY(cfg.generateNonReadSectionKeyPairs().isEmpty());
    }


private slots:
    void tRead() {

        QTemporaryFile file;
        QVERIFY(file.open());
        QTextStream stream(&file);
        stream << cfg;
        file.close();

        Cfg cfg;
        // parse, verify, store and verify again
        cfg.parse(file.fileName());
        verifyCfg(cfg);
        cfg.store(file.fileName());

        cfg.parse(file.fileName());
        verifyCfg(cfg);
    }

};

DECLARE_TEST(CfgTest)

#include "test_cfg.moc"
